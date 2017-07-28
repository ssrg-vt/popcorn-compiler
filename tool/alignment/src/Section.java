

public class Section<X, Y, Z> { 
  public X x; //name
  public Y y; // addr 
  public Z z; // size
  
  
  public Section(X x, Y y, Z z) { 
    this.x = x; 
    this.y = y; 
    this.z = z;
  }
  
  @Override
  public String toString() {
      return "(" + x + ',' + y + ',' + z + ')';
  }
  
  /************* Get ***************************/
  public String getName(){
	  return (String) x;
  }
  
  public Y getAddr(){
	  return y;
  }
  
  public Z getSize(){
	  return z;
  }
    
  /************* Set ************************/
  public void setName(X name){
	  this.x = name;
  }
    
  public void setAddr(Y addr){
	  this.y = addr;
  }
  
  public void setSize(Z size){
	  this.z = size;
  }
} 