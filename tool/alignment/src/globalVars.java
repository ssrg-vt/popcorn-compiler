import java.util.ArrayList;
import java.util.List;


public class globalVars {

    public enum Section {
	COMMON, DATA, RODATA
    }
											//static int symbol_offset =256;  // 0x50 
	public static int symbol_offset = 100;
											// 0x500
    public static int offset = 1000;	// 0x50

    public static boolean DEBUG = true;
    public static boolean hasTLS_DATA = false;
    public static boolean hasTLS_BSS = false;

    //format is :   Name, X86 SIZE, ARM SIZE
    public static List < Tuple < String, Long, Long, Long >> A_text =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    public static List < Tuple < String, Long, Long, Long >> A_rodata =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    public static List < Tuple < String, Long, Long, Long >> A_data =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    //public static List<Tuple<String,Long,Long,Long>> A_data_Relrolocal = new ArrayList<Tuple<String,Long,Long,Long>>();
    public static List < Tuple < String, Long, Long, Long >> A_bss =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    //TLS stuff
    public static List < Tuple < String, Long, Long, Long >> A_data_TLS =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    public static List < Tuple < String, Long, Long, Long >> A_bss_TLS =
	new ArrayList < Tuple < String, Long, Long, Long >> ();

    public static List < String > Antonio_BlackList =
	new ArrayList < String > ();
    public static List < String > Antonio_WhiteList =
	new ArrayList < String > ();
    public static List < String > Antonio_YellowList =
	new ArrayList < String > ();

    static void resetSectionsInfo() {
	long default_size = 0;
	int p;

	for (p = 0; p < A_text.size(); p++) {
	    A_text.get(p).setSize_x86_64(default_size);
	    A_text.get(p).setSize_aarch64(default_size);
	}
	//RODATA 
	for (p = 0; p < A_rodata.size(); p++) {
	    A_rodata.get(p).setSize_x86_64(default_size);
	    A_rodata.get(p).setSize_aarch64(default_size);
	}
	//DATA
	for (p = 0; p < A_data.size(); p++) {
	    A_data.get(p).setSize_x86_64(default_size);
	    A_data.get(p).setSize_aarch64(default_size);
	}
	//BSS
	for (p = 0; p < A_bss.size(); p++) {
	    A_bss.get(p).setSize_x86_64(default_size);
	    A_bss.get(p).setSize_aarch64(default_size);
	}
	//TLS data
	for (p = 0; p < A_data_TLS.size(); p++) {
	    A_data_TLS.get(p).setSize_x86_64(default_size);
	    A_data_TLS.get(p).setSize_aarch64(default_size);
	}
	//TLS BSS
	for (p = 0; p < A_bss_TLS.size(); p++) {
	    A_bss_TLS.get(p).setSize_x86_64(default_size);
	    A_bss_TLS.get(p).setSize_aarch64(default_size);
	}
    }

    static void resetMultipleAddress() {
	int default_size = 0;
	int p;

	for (p = 0; p < A_text.size(); p++) {
	    A_text.get(p).setMultAddressFlag(default_size);

	}
	//RODATA
	for (p = 0; p < A_rodata.size(); p++) {
	    A_rodata.get(p).setMultAddressFlag(default_size);

	}
	//DATA
	for (p = 0; p < A_data.size(); p++) {
	    A_data.get(p).setMultAddressFlag(default_size);
	}
	//BSS
	for (p = 0; p < A_bss.size(); p++) {
	    A_bss.get(p).setMultAddressFlag(default_size);
	}
	//BSS
	for (p = 0; p < A_data_TLS.size(); p++) {
	    A_data_TLS.get(p).setMultAddressFlag(default_size);
	}
	//BSS
	for (p = 0; p < A_bss_TLS.size(); p++) {
	    A_bss_TLS.get(p).setMultAddressFlag(default_size);
	}
    }
}
