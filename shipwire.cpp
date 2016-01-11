#include <iostream>
#include <map>
#include <deque>
#include <sstream>
#include <fstream>

typedef char productKey; // using typedef here since this looks like something that will likely need to be expanded to a broader type, for ex std::string

/* Represents the inventory. Keeping this to be a very dumb object that only knows objects 
   are going in and out. No real business logic. 
   Likely future changes:
	- adding products, possibly a very large number of products
	- adding a re-stocking interface
	- multiple warehouses (and splitting orders across)
	- partial fulfilment of orders
	- persistent storage
	- concurrency
*/
class Warehouse{
	std::map<productKey,unsigned> Stores;
	unsigned TotalInventory; 		// used to indicate empty	
public:
	Warehouse() : TotalInventory(0) {}

	/* Check if the warehouse contains NO products 
	*/
	bool IsEmpty() { 
		return (TotalInventory==0); 
	} /* IsEmpty */

	/* The specification doesn't require it but it seems sensible to provide 
	   capability to increase the inventory instead of just making it a special
	   case in the constructor. This seems to be a likely future need.
	*/
	void Store( productKey UnitType, unsigned Count ){
		Stores[UnitType] += Count;
		TotalInventory += Count;
	} /* Store */				

	/* Removes the requested count, returning true if there was enough. 
	   If not enough returns false. 
	   Pull an "illegal" product is not an error here. 
	   A version to return partial would need to return the count and this version
	   would end up being a special case.
	*/
	bool Pull( productKey UnitType, unsigned Count ){
		if( Stores[UnitType] < Count ){ 
			return false;
		}
		Stores[UnitType] -= Count;
		TotalInventory -= Count;
		return true;
	} /* Pull */

	/* Determines if a given product is a legal member of the warehouse
	*/
	bool CheckProduct( productKey Product ){
		return ( Stores.find(Product) != Stores.end() );
	} /* CheckProduct */

	/* Dump out the current contents of the warehouse. Used for debugging 
	*/
	void Dump(){
		for( std::map<productKey,unsigned>::const_iterator it = Stores.begin();
			it != Stores.end();	
			++it
		){
			std::cout << it->first << it->second;		
		}
		std::cout << std::endl;
	} /* Dump */

}; // class Warehouse

/* The subset of an order corresponding to a single product. 
   Both the requests and the results.
   Note that legality of the Product is not check in this class. It only really 
   cares what it has been asked to do and an illegal product doesn't violate this.
*/
class OrderLine {
	productKey Product;
	unsigned Requested;
	unsigned Pulled;
	unsigned Backlog;

public:
	OrderLine( productKey Product, unsigned Requested ) : Product(Product), Requested(Requested) {}

	/* Attempt to fulfill this element of the order with the warehouse.
	   Non-exsisting or illegal products cannot be allocated and attempts do not generate
	   errors. 
	*/
	void Allocate( Warehouse &Inventory ){
		if( Inventory.Pull(Product,Requested) ){
			Pulled = Requested;
			Backlog = 0;
		}else{
			Pulled = 0;
			Backlog = Requested;
		}				
	} /* Allocate */

	/* Output the request amount of the product plus the results 
	*/
	void Dump() const {
		std::cout << Product << "=" << Requested << '/' << Pulled << '%' << Backlog;
	} /* Dump */

}; // class OrderLine

/* An order on a specific stream. Note that class doesn't care if the stream uses the same 
   header more than once. While this violates the rules of the system I have elected to 
   keep this responsibility in the parsing code since there doesn't seem to be any adverse 
   impact at this level.
*/
class Order {
	std::string Stream;
	std::string Header;
	std::deque<OrderLine> Lines;

public:
	Order( std::string Stream, std::string Header ) : Stream(Stream), Header(Header) {}
	
	/* Add the request to order a specific product + quantity 
	*/ 
	void Add( OrderLine Line ){
		Lines.push_back( Line );
	} /* Add */

	/* Output all the elements of this order. Order of products listed not important. 
	*/
	void Dump() const {
		std::cout << Stream << "-" << Header << ": ";
	  	for(
			std::deque<OrderLine>::const_iterator it = Lines.cbegin();
			it != Lines.cend();
			++it
		){			
			if( it != Lines.begin() ) std::cout << ", ";
			it->Dump();
		}
		std::cout << std::endl;	
	} /* Dump */

}; // class Order

/* The history of orders that have been processed, including results
   A deque was selected here due to its inherent pagination since this 
   could grow rather large. A likely change would be to output this data 
   to persistent storage asynchronously. This will be strongly impacted
   by the needs to access the information.
*/
class OrderLog {
	std::deque<Order> Orders; 

public:
	/* Record a processed order
	*/
	void Add( Order Order ){
		Orders.push_back(Order); 
	} /* Add */

	/* Output all of the orders processed by the system.
	*/
	void Dump() const {
	  	for(
			std::deque<Order>::const_iterator it = Orders.cbegin();
			it != Orders.cend();
			++it
		){			
			it->Dump();
		}		
	} /* Dump */

}; // class OrderLog

class OrderParser {
	Warehouse Inventory;
	OrderLog Log;

private:
	/* This checks if the given Line represents a valid order. 
	   The structure mimics that of Parser() and should be kept in synch.
	*/
	bool Check( std::string Line ){
		std::istringstream ss(Line);
		std::string stream, header;
		ss >> stream >> header;
		if( stream.length() == 0 || header.length() == 0 )
 			return false;
		Order order(stream,header);
		productKey product;
		unsigned quantity;
		bool hasProduct = false;
		while( ss >> product >> quantity ){
			if( quantity > 0 ) hasProduct = true;
			if( quantity > 5 ) return false;
			if( ! Inventory.CheckProduct(product) ) return false;
		}
		if( ! hasProduct )
			return false; // an order isn't valid unless theres 1 product 0-5 count
		return true;
	} /* Check */

public:
	OrderParser(){
		Inventory.Store('A',3);
		Inventory.Store('B',3);
	} /* constructor */

	/* Initializes warehouse with contents of file. This is kind of hacked together
	   with no error checking.
	*/
	OrderParser( std::string InventoryFilename ){
		std::ifstream file(InventoryFilename);
		std::string line;
		std::getline(file,line);
		std::istringstream ss(line);
		productKey product;
		unsigned quantity;
		while( ss >> product >> quantity )
			Inventory.Store(product,quantity);
	} /* constructor */
	
	/* Parses an order from the datasouce and applies it. 
	   The structure mimics that of Check() and should be kept in synch.
	*/
	void Parse( std::string Line ){
		if( ! Check(Line) ) // check for valid order
			return;
		std::istringstream ss(Line);
		std::string stream, header;
		ss >> stream >> header;
		Order order(stream,header);
		productKey product;
		unsigned quantity;
		while( ss >> product >> quantity ){
			OrderLine line(product,quantity);
			line.Allocate(Inventory);
			order.Add(line);	
		}
		Log.Add(order);	
	} /* Parse */

	void ParseFile( std::string Filename ){
		std::ifstream file(Filename);
		if(! file) return;
		std::string line;
		for(;;){
			while(std::getline(file,line)){
				Parse(line);
				if( Inventory.IsEmpty() ){
					Log.Dump();
					return;
				}
				// should add a sleep here but not portable
			} 
			file.clear();
		} // forever	
	} /* ParseFile */
		
}; // class OrderParser

int main( int argc, char* argv[] ){
	if( argc < 3 ){
		std::cout << "USAGE:" << std::endl;
		std::cout << argv[0] << " inventory_file orders_file" << std::endl;
		return -1;
	}

	OrderParser parser(argv[1]);
	parser.ParseFile(argv[2]);

	return 0;
}





