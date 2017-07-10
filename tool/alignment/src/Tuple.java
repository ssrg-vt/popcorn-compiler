

public class Tuple < X, Y, Z, W > {
    public X x;			//name
    public Y y;			// x86 
    public Z z;			// arm
    public W w;			// addr
    public int flag;
    public long alignment_x86;
    public long alignment_aRM;
    public int numCounter;
    public int sameSymbolname_diffAddr;
    public int sameSymbolname_diffAddr_ARM;


    public Tuple(X x, Y y, Z z, W w, long u, long v) {
	this.x = x;
	this.y = y;
	this.z = z;
	this.w = w;
	this.flag = -1;
	this.alignment_x86 = u;
	this.alignment_aRM = v;
	this.numCounter = 0;
	this.sameSymbolname_diffAddr = 0;
	this.sameSymbolname_diffAddr_ARM = 0;
    } @Override public String toString() {
	return "(" + x + ',' + y + ',' + z + ')';
    }

  /************* Get ***************************/
    public String getName() {
	return (String) x;
    }

    public Y getSize_x86_64() {
	return y;
    }

    public Z getSize_aarch64() {
	return z;
    }

    public W getAddr() {
	return w;
    }

    public int getFlag() {
	return flag;
    }

    public int getCount() {
	return numCounter;
    }

    public long getAlignment_x86() {
	return alignment_x86;
    }
    public long getAlignment_aRM() {
	return alignment_aRM;
    }
    public int getMultAddressFlag() {
	return sameSymbolname_diffAddr;
    }
    public int getMultAddressFlag_ARM() {
	return sameSymbolname_diffAddr_ARM;
    }

  /************* Set ************************/
    public void setName(X name) {
	this.x = name;
    }

    public void setSize_x86_64(Y size) {
	this.y = size;
    }

    public void setSize_aarch64(Z size) {
	this.z = size;
    }

    public void setAddr(W addr) {
	this.w = addr;
    }

    public void setFlag(int newVal) {
	this.flag = newVal;
    }

    public void incrementCount() {
	this.numCounter += 1;
    }

    public void setAlignment_x86(long myalign) {
	this.alignment_x86 = myalign;
    }
    public void setAlignment_aRM(long myalign) {
	this.alignment_aRM = myalign;
    }
    public void setMultAddressFlag(int num) {
	this.sameSymbolname_diffAddr = num;
    }

    public void updateBy1_MultAddressFlag() {
	this.sameSymbolname_diffAddr += 1;
    }
    public void updateBy1_MultAddressFlag_ARM() {
	this.sameSymbolname_diffAddr_ARM += 1;
    }

  /************AddTo Size**************************/
  /********* >> use combination of get and set ( + is not defined  ) */
}
