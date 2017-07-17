import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class AlignmentLogic {
	/**
	 * 0 x86 text
	 * 1 x86 rodata
	 * 2 x86 data
	 * 3 x86 bss
	 * 4 x86 TDATA
	 * 5 x86 TBSS
	 * 
	 * 6 arm text
	 * 7 arm rodata
	 * 8 arm data
	 * 9 arm bss
	 * 10 arm TDATA
	 * 11 arm TBSS
	 */
    public static List < Section < String, Long, Long >> RangesInfo =
	new ArrayList < Section < String, Long, Long >> ();

    public static List < Tuple < String, Long, Long, Long >> A_x86only =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    public static List < Tuple < String, Long, Long, Long >> A_aRMonly =
	new ArrayList < Tuple < String, Long, Long, Long >> ();
    static int textCount = 0;
    static int rodataCount = 0;
    static int dataRelroCount = 0;
    static int dataCount = 0;
    static int bssCount = 0;

	/* Pierre: print the content of RangesInfo */
	public static void plogRangesInfo() {
		Iterator it = RangesInfo.iterator();
		
		Plog.log("name\taddress\tsize\n");
		Plog.log("-------------------\n");

		while(it.hasNext()) {
			Section < String, Long, Long> element = (Section)it.next();
			Plog.log(element.getName() + "\t" + element.getAddr() + "\t" +
				element.getSize() + "\n");
		}	
	}

	/** recordRanges
	 * @param option:
	 *		0: x86
	 * 		1: aarch64
	 * @throws IOException
	 * @throws InterruptedException
	 * This function opens and reads the readelf text files in order to obtain 
	 * the address ranges for each section of the binary
	 */
    static void recordRanges(int option) throws IOException,
	InterruptedException {
	//run readelf to txt file
	FileReader fr1;
	 List < String > readelf_output = new ArrayList < String > ();
	String temp;

	if (option == 0) {
	    fr1 =
		new FileReader(new
			       File(Runtime.TARGET_DIR + "/" +
				    "readelf_x86n.txt"));
	} else if (option == 1) {
	    fr1 =
		new FileReader(new
			       File(Runtime.TARGET_DIR + "/" +
				    "readelf_aRMn.txt"));
	} else {
	    System.out.println("Invalid option selected");
	    throw new IllegalArgumentException();
	}

	//read in file
	BufferedReader br1 = new BufferedReader(fr1);
	//read into array 1
	while ((temp = br1.readLine()) != null) {
	    readelf_output.add(temp);
	}
	br1.close();
	for (int a = 0; a < readelf_output.size(); a++) {
	    Pattern p =
		Pattern.compile
		("[ \t]*[[ 0-9]*][ \t]*([\\._\\-\\@A-Za-z0-9]*)[ \t]*[_\\-A-Z]*[ \t]+([0-9a-fA-F]*)[ \t]+[0-9a-fA-f]*[ \t]+([0-9a-fA-f]*)[ \t]*[0-9]+[ \t]*[ A-Z]*[ \t]*[0-9][ \t]+[0-9]+[ \t]+[0-9]*");
	    Matcher m = p.matcher(readelf_output.get(a));
	    while (m.find()) {
		//System.out.println("Name: "+m.group(1)+"\t\t\tAddr: "+m.group(2)+"\t\tSize: "+m.group(3));
		//TEXT
		if (m.group(1).compareTo(".text") == 0) {
		    RangesInfo.add(new Section < String, Long,
				   Long > (m.group(1),
					   Long.parseLong(m.group(2), 16),
					   Long.parseLong(m.group(3),
							  16)));
		    if (globalVars.DEBUG)
			System.out.println("Name: " + m.group(1) +
					   "    Adeed" + "  0x" +
					   m.group(2) + "    0x" +
					   m.group(3));
		}
		//RODATA
		else if (m.group(1).compareTo(".rodata") == 0) {
		    RangesInfo.add(new Section < String, Long,
				   Long > (m.group(1),
					   Long.parseLong(m.group(2), 16),
					   Long.parseLong(m.group(3),
							  16)));
		    if (globalVars.DEBUG)
			System.out.println("Name: " + m.group(1) +
					   "    Adeed" + "  0x" +
					   m.group(2) + "    0x" +
					   m.group(3));
		}
		//DATA
		else if (m.group(1).compareTo(".data") == 0) {
		    RangesInfo.add(new Section < String, Long,
				   Long > (m.group(1),
					   Long.parseLong(m.group(2), 16),
					   Long.parseLong(m.group(3),
							  16)));
		    if (globalVars.DEBUG)
			System.out.println("Name: " + m.group(1) +
					   "    Adeed" + "  0x" +
					   m.group(2) + "    0x" +
					   m.group(3));
		}
		//BSS
		else if (m.group(1).compareTo(".bss") == 0) {
		    RangesInfo.add(new Section < String, Long,
				   Long > (m.group(1),
					   Long.parseLong(m.group(2), 16),
					   Long.parseLong(m.group(3),
							  16)));
		    if (globalVars.DEBUG)
			System.out.println("Name: " + m.group(1) +
					   "    Adeed" + "  0x" +
					   m.group(2) + "    0x" +
					   m.group(3));
		}
		//TDATA (TLS)
		else if (m.group(1).compareTo(".tdata") == 0) {
		    RangesInfo.add(new Section < String, Long,
				   Long > (m.group(1),
					   Long.parseLong(m.group(2), 16),
					   Long.parseLong(m.group(3),
							  16)));
		    if (globalVars.DEBUG)
			System.out.println("Name: " + m.group(1) +
					   "    Adeed" + "  0x" +
					   m.group(2) + "    0x" +
					   m.group(3));
		    globalVars.hasTLS_DATA = true;
		}
		//TBSS (TLS)
		else if (m.group(1).compareTo(".tbss") == 0) {
		    RangesInfo.add(new Section < String, Long,
				   Long > (m.group(1),
					   Long.parseLong(m.group(2), 16),
					   Long.parseLong(m.group(3),
							  16)));
		    if (globalVars.DEBUG)
			System.out.println("Name: " + m.group(1) +
					   "    Adeed" + "  0x" +
					   m.group(2) + "    0x" +
					   m.group(3));
		    globalVars.hasTLS_BSS = true;
		}
	    }			//END while(m.find())
	}			//END FOR readelf file
	if (RangesInfo.isEmpty()) {
	    System.out.println
		("Could not obtain Ranges Info. Did Program Successfully compile?");
	    throw new IOException();
	}
	if (!globalVars.hasTLS_DATA)
	    RangesInfo.add(new Section < String, Long,
			   Long > ("TOTALLY NULL DATA", (long) 0,
				   (long) 0));
	if (!globalVars.hasTLS_BSS)
	    RangesInfo.add(new Section < String, Long,
			   Long > ("TOTALLY NULL BSS", (long) 0,
				   (long) 0));
	if (globalVars.DEBUG)
	    System.out.println("");
    }				//END recordRanges

	/**
	 * @throws InterruptedException
	 * @throws IOException
	 * Perform non-intersection removal, ADD unique TO SEPARATE array lists!
	 */
    static void cleanIntersection() throws InterruptedException,
	IOException {
	int p;

	//TEXT
	for (p = 0; p < globalVars.A_text.size(); ++p) {
	    if (globalVars.A_text.get(p).getSize_x86_64() == 0
		|| globalVars.A_text.get(p).getSize_aarch64() == 0) {
		if (globalVars.DEBUG)
		    System.out.println("Removed Text<<" +
				       globalVars.A_text.get(p).getName() +
				       ">> x86Sz:0x" +
				       Long.toHexString(globalVars.A_text.
							get
							(p).getSize_x86_64
							()) +
				       "\taRMSz:0x" +
				       Long.toHexString(globalVars.A_text.
							get
							(p).getSize_aarch64
							()));
		if (globalVars.A_text.get(p).getSize_x86_64() != 0
		    && globalVars.A_text.get(p).getSize_aarch64() == 0) {
		    A_x86only.add(globalVars.A_text.get(p));
		} else if (globalVars.A_text.get(p).getSize_x86_64() == 0
			   && globalVars.A_text.get(p).getSize_aarch64() !=
			   0) {
		    A_aRMonly.add(globalVars.A_text.get(p));
		} else {
		    if (globalVars.DEBUG)
			System.out.println("Double NULL!!: " +
					   globalVars.A_text.get(p).
					   getName());
		}
		globalVars.A_text.remove(p);
		--p;
	    }
	}
	//RODATA
	for (p = 0; p < globalVars.A_rodata.size(); p++) {
	    if (globalVars.A_rodata.get(p).getSize_x86_64() == 0
		|| globalVars.A_rodata.get(p).getSize_aarch64() == 0
		|| globalVars.A_rodata.get(p).getAddr() == 0) {
		if (globalVars.DEBUG)
		    System.out.println("Removed Rodata<<" +
				       globalVars.A_rodata.get(p).
				       getName() + ">> x86Sz:0x" +
				       Long.toHexString(globalVars.
							A_rodata.get
							(p).getSize_x86_64
							()) +
				       "\taRMSz:0x" +
				       Long.toHexString(globalVars.
							A_rodata.get
							(p).getSize_aarch64
							()));
		if (globalVars.A_rodata.get(p).getSize_x86_64() != 0
		    && globalVars.A_rodata.get(p).getSize_aarch64() == 0) {
		    A_x86only.add(globalVars.A_rodata.get(p));
		} else if (globalVars.A_rodata.get(p).getSize_x86_64() == 0
			   && globalVars.A_rodata.get(p).
			   getSize_aarch64() != 0) {
		    A_aRMonly.add(globalVars.A_rodata.get(p));
		} else {
		    if (globalVars.DEBUG)
			System.out.println("Double NULL!!: " +
					   globalVars.A_rodata.get(p).
					   getName());
		}
		globalVars.A_rodata.remove(p);
		--p;
	    }
	}
	//DATA
	for (p = 0; p < globalVars.A_data.size(); p++) {
	    if (globalVars.A_data.get(p).getSize_x86_64() == 0
		|| globalVars.A_data.get(p).getSize_aarch64() == 0) {
		if (globalVars.DEBUG)
		    System.out.println("Removed Data<<" +
				       globalVars.A_data.get(p).getName() +
				       ">> x86Sz:0x" +
				       Long.toHexString(globalVars.A_data.
							get
							(p).getSize_x86_64
							()) +
				       "\taRMSz:0x" +
				       Long.toHexString(globalVars.A_data.
							get
							(p).getSize_aarch64
							()));
		if (globalVars.A_data.get(p).getSize_x86_64() != 0
		    && globalVars.A_data.get(p).getSize_aarch64() == 0) {
		    A_x86only.add(globalVars.A_data.get(p));
		} else if (globalVars.A_data.get(p).getSize_x86_64() == 0
			   && globalVars.A_data.get(p).getSize_aarch64() !=
			   0) {
		    A_aRMonly.add(globalVars.A_data.get(p));
		} else {
		    if (globalVars.DEBUG)
			System.out.println("Double NULL!!: " +
					   globalVars.A_data.get(p).
					   getName());
		}
		globalVars.A_data.remove(p);
		--p;
	    }
	}
	//BSS
	for (p = 0; p < globalVars.A_bss.size(); p++) {
	    if (globalVars.A_bss.get(p).getSize_x86_64() == 0
		|| globalVars.A_bss.get(p).getSize_aarch64() == 0) {
		System.out.println("Removed Bss<<" +
				   globalVars.A_bss.get(p).getName() +
				   ">> x86Sz:0x" +
				   Long.toHexString(globalVars.A_bss.
						    get(p).getSize_x86_64
						    ()) + "\taRMSz:0x" +
				   Long.toHexString(globalVars.A_bss.
						    get(p).getSize_aarch64
						    ()));
		if (globalVars.A_bss.get(p).getSize_x86_64() != 0
		    && globalVars.A_bss.get(p).getSize_aarch64() == 0) {
		    A_x86only.add(globalVars.A_bss.get(p));
		} else if (globalVars.A_bss.get(p).getSize_x86_64() == 0
			   && globalVars.A_bss.get(p).getSize_aarch64() !=
			   0) {
		    A_aRMonly.add(globalVars.A_bss.get(p));
		} else {
		    System.out.println("Double NULL!!: " +
				       globalVars.A_bss.get(p).getName());
		}
		globalVars.A_bss.remove(p);
		--p;
	    }
	}
	if (globalVars.DEBUG)
	    System.out.println("\nA_text SIZE CLEAN: " +
			       globalVars.A_text.size());
    }				//END cleanIntersection

    /************************************************************************/
    /* set_symbol_Alignment                                                 */
	/************************************************************************/
	static void set_symbol_Alignment(List < Tuple < String, Long, Long,
				     Long >> A_sectionList,
				     List < String > A_x86_alignment,
				     List < String > A_aarch64_alignment,
				     boolean defaultAlign) throws
	InterruptedException, IOException {
	if (defaultAlign) {
	    //offset so they are aligned
	    for (int i = 0; i < A_sectionList.size(); i++) {
		//System.out.println("*(."+A_sectionList.get(i).getName()+");");
		A_x86_alignment.add("\t . = ALIGN(0x" +
				    globalVars.symbol_offset + ");");
		A_x86_alignment.add("*(" + A_sectionList.get(i).getName() +
				    ");");
		A_aarch64_alignment.add("\t . = ALIGN(0x" +
					globalVars.symbol_offset + ");");
		A_aarch64_alignment.add("*(" +
					A_sectionList.get(i).getName() +
					");");
	    }
	} else
	    //offset so they are aligned
	    for (int i = 0; i < A_sectionList.size(); i++) {
		//System.out.println("*(."+A_sectionList.get(i).getName()+");");
		A_x86_alignment.add("*(" + A_sectionList.get(i).getName() +
				    ");");
		A_aarch64_alignment.add("*(" +
					A_sectionList.get(i).getName() +
					");");
	    }
    }				//end set_symbol_Alignment

	/*************************************************************************/
    /* long_alignSection                                                     */
    /*************************************************************************/
    static long alignSection(String section) throws IOException,
	InterruptedException {
	int x86_addr = 0, arm_addr = 0;
	String temp, sectionSymbol = "";

	if (section.compareTo("rodata") == 0) {
	    sectionSymbol = "bmask";
	} else if (section.compareTo("data") == 0) {
	    sectionSymbol = "_global_impure_ptr";
	} else if (section.compareTo("bss") == 0) {
	    sectionSymbol = "__bss_start";
	} else if (section.compareTo("rodata") == 0) {
	    sectionSymbol = "???";
	}
	//open the x86 symbol nm file
	FileReader fr1 =
	    new FileReader(new
			   File(Runtime.TARGET_DIR +
				"/nm_x86_64_musl.txt"));
	BufferedReader br1 = new BufferedReader(fr1);
	//read into array
	while ((temp = br1.readLine()) != null) {
	    if (Pattern.compile(Pattern.quote(sectionSymbol),
				Pattern.CASE_INSENSITIVE).matcher(temp).
		find()) {
		//System.out.println("-------x86 END LINE:"+temp);
		String x86hex_funcAddr = temp.substring(0, 16);
		x86_addr = Integer.parseInt(x86hex_funcAddr, 16);
		//System.out.println("-------x86 END:"+x86_addr);
	    }
	}
	//open the arm symbol nm file
	fr1 =
	    new FileReader(new
			   File(Runtime.TARGET_DIR +
				"/nm_aarch64_musl.txt"));
	br1 = new BufferedReader(fr1);
	//read into array
	while ((temp = br1.readLine()) != null) {
	    if (Pattern.compile(Pattern.quote(sectionSymbol),
				Pattern.CASE_INSENSITIVE).matcher(temp).
		find()) {
		//System.out.println("-------AARCH TEXT END LINE:"+temp);
		String armhex_funcAddr = temp.substring(0, 16);
		arm_addr = Integer.parseInt(armhex_funcAddr, 16);
		//System.out.println("-------AARCH TEXT END:"+arm_addr);
	    }
	}
	br1.close();
	fr1.close();
	return (x86_addr - arm_addr);
	//positive value = ARM is shorter and needs ARM to be extended
	//negative value = x86 is shorter and needs x86 to be extended
    }

	/*************************************************************************/
    /* checkSameAddr_MultipleSymbol                                          */
    /*************************************************************************/
    static void checkSameAddr_MultipleSymbol(List < Tuple < String, Long,
					     Long, Long >> currList,
					     Long addr) throws IOException,
	InterruptedException {
	//currList.get(currList.size()-1)  >> newest element
	//currList.get(currList.size()-2)  >> previous element 
	if (currList.size() > 1) {
	    if (currList.get(currList.size() - 1).getAddr() == addr) {
		currList.get(currList.size() - 1).setFlag(1);
		if (globalVars.DEBUG)
		    System.out.
			println("######### !!!sameAddr_MultipleSymbol: " +
				currList.get(currList.size() -
					     1).getName());
	    }
	}
    } 

	/*************************************************************************/
	/* NOT USED */
	/*************************************************************************/
	static void checkSameSymbol_MultipleAddress() {
	//if this is true, need to change symbol size to be size + alignment fill
    }


	/*************************************************************************/
	/* add_sectionAlignment                                                  */
    /*************************************************************************/
	/**
	 * ADD in PADDING AT END OF SECTION IF NEEDED.
	 * @param section
	 */
    static void add_sectionAligment(String section,
				    long additionalsneaky) throws
	IOException, InterruptedException {
	//Clear RangesInfo
	resetRangesInfo();
	//get current ELF info
	recordRanges(0);
	recordRanges(1);

	int s_x86, s_arm, x86_insertPT, arm_insertPT;

	switch (section) {
	case "text":
	    s_x86 = 0;
	    s_arm = 6;
	    x86_insertPT =
		LinkerIO.x86_textendLine_Offset +
		LinkerIO.getLinkerNewLines_x86();
	    arm_insertPT =
		LinkerIO.aarch64_textendLine_Offset +
		LinkerIO.getLinkerNewLines_aRM();
	    break;
	    case "rodata":s_x86 = 1;
	    s_arm = 7;
	    x86_insertPT =
		LinkerIO.x86_rodataendLine_Offset +
		LinkerIO.getLinkerNewLines_x86();
	    arm_insertPT =
		LinkerIO.aarch64_rodataendLine_Offset +
		LinkerIO.getLinkerNewLines_aRM();
	    break;
	    case "data":s_x86 = 2;
	    s_arm = 8;
	    x86_insertPT =
		LinkerIO.x86_dataendLine_Offset +
		LinkerIO.getLinkerNewLines_x86();
	    arm_insertPT =
		LinkerIO.aarch64_dataendLine_Offset +
		LinkerIO.getLinkerNewLines_aRM();
	    break;
	    case "bss":s_x86 = 3;
	    s_arm = 9;
	    x86_insertPT =
		LinkerIO.x86_bssendLine_Offset +
		LinkerIO.getLinkerNewLines_x86();
	    arm_insertPT =
		LinkerIO.aarch64_bssendLine_Offset +
		LinkerIO.getLinkerNewLines_aRM();
	    break;
	    case "tdata":s_x86 = 4;
	    s_arm = 10;
	    x86_insertPT =
		LinkerIO.x86_tlsdataendLine_Offset +
		LinkerIO.getLinkerNewLines_x86();
	    arm_insertPT =
		LinkerIO.aarch64_tlsdataendLine_Offset +
		LinkerIO.getLinkerNewLines_aRM();
	    case "tbss":s_x86 = 5;
	    s_arm = 11;
	    x86_insertPT =
		LinkerIO.x86_tlsbssendLine_Offset +
		LinkerIO.getLinkerNewLines_x86();
	    arm_insertPT =
		LinkerIO.aarch64_tlsbssendLine_Offset +
		LinkerIO.getLinkerNewLines_aRM();
	    default:throw new IllegalArgumentException("Invalid Section: "
						       + section);
	} long sectMax_x86 =
	    RangesInfo.get(s_x86).getAddr() +
	    RangesInfo.get(s_x86).getSize();
	long sectMax_arm =
	    RangesInfo.get(s_arm).getAddr() +
	    RangesInfo.get(s_arm).getSize();
	if (globalVars.DEBUG)
	    System.
		out.println
		("\n------------------------ Add_end of section Alignment<<"
		 + section + ">>: Max x86: " + sectMax_x86 + " Max arm: " +
		 sectMax_arm);
	//System.out.println("@@@@size:"+AlignmentLogic.RangesInfo.get(0).getSize_x86_64() + "++"+AlignmentLogic.RangesInfo.get(0).getSize_aarch64());
	//System.out.println("@@@@size:"+AlignmentLogic.RangesInfo.get(4).getSize_x86_64() + "++"+AlignmentLogic.RangesInfo.get(4).getSize_aarch64());
	long sizeDif = sectMax_x86 - sectMax_arm;
	if (globalVars.DEBUG)
	    System.out.println(section + " DIFF: " + sizeDif +
			       "plus sneaky: " + additionalsneaky);
	if (sizeDif < 0) {
	    //ARM LARGER add padding to x86
	    if (globalVars.DEBUG)
		System.out.println("<<Padding x86>>");
	    LinkerIO.sectionSize_alignment.add("\t" + " . = . + 0x" +
					       Long.
					       toHexString(Math.abs
							   (sizeDif) +
							   additionalsneaky)
					       + "; /*END OF SECTION " +
					       section + " SZ ALIGN*/");
	    LinkerIO.addAlignmentToLinkerScripts(LinkerIO.
						 sectionSize_alignment,
						 null, x86_insertPT, 0);
	} else if (sizeDif > 0) {
	    //x86 LARGER add padding to ARM
	    if (globalVars.DEBUG)
		System.out.println("<<Padding ARM>>");
	    LinkerIO.sectionSize_alignment.add("\t" + " . = . + 0x" +
					       Long.
					       toHexString(Math.abs
							   (sizeDif) +
							   additionalsneaky)
					       + "; /*END OF SECTION " +
					       section + " SZ ALIGN*/");
	    LinkerIO.addAlignmentToLinkerScripts(null,
						 LinkerIO.
						 sectionSize_alignment, 0,
						 arm_insertPT);
	} else {		//Have a nice day!
	}
	LinkerIO.sectionSize_alignment.clear();
    }				//END add_sectionAligment

	/**************************************************************************/
	/** compareAndSet_Alignment()                                             */
	/**************************************************************************/
    static void compareAndSet_Alignment(List < Tuple < String, Long, Long,
					Long >> symbols, int x86_offset,
					int aarch64_offset) throws
	IOException {
	if (symbols.isEmpty())
	    return;

	String temp;
	if (globalVars.DEBUG)
	     System.out.println("\n\ninside CAS " + symbols.get(0));
	// find where first rodata symbol is in both x86-64/aarch64
	if (!symbols.isEmpty()) {
	    //open and grab x86 address of first symbol
	    FileReader fr1 =
		new FileReader(new
			       File(Runtime.TARGET_DIR +
				    "/nm_symbols_x86.txt"));
	    BufferedReader br1 = new BufferedReader(fr1);
	    while ((temp = br1.readLine()) != null) {
		//System.out.println(temp);
		if (temp.substring(19).equals(symbols.get(0).getName())) {
		    symbols.get(0).
			setSize_x86_64(Long.parseLong
				       (temp.substring(0, 16), 16));
		    if (globalVars.DEBUG)
			System.out.println(symbols.get(0).
					   getSize_x86_64());
		    break;
		}
	    }
	    //open and grab aarch64 address of first symbol fr1 =
		new FileReader(new
			       File(Runtime.TARGET_DIR +
				    "/nm_symbols_aarch64.txt"));
	    br1 = new BufferedReader(fr1);
	    while ((temp = br1.readLine()) != null) {
		if (temp.substring(19).equals(symbols.get(0).getName())) {
		    symbols.get(0).
			setSize_aarch64(Long.parseLong
					(temp.substring(0, 16), 16));
		    if (globalVars.DEBUG)
			System.out.println(symbols.get(0).
					   getSize_aarch64());
		    break;
		}
	    }
	    br1.close();

	    //compare and create align line
	    long diff =
		symbols.get(0).getSize_x86_64() -
		symbols.get(0).getSize_aarch64();
	    if (diff > 0) {
		//x86_64 has larger address, need to offset aarch64
		String align_aarch64 =
		    "\t" + " . = . + 0x" +
		    Long.toHexString(Math.abs(diff)) + ";";
		if (globalVars.DEBUG)
		    System.out.println("CAS \t" + " . = . + 0x" +
				       Long.toHexString(Math.abs(diff)) +
				       ";");
		LinkerIO.addLineToLinkerScript(align_aarch64,
					       aarch64_offset + 1, 1);
		if (globalVars.DEBUG)
		    System.out.println("ARM:::: imserted in line: " +
				       aarch64_offset);
	    } else if (diff < 0) {
		//aarch64 has larger address, need to offset x86_64
		String align_x86_64 =
		    "\t" + " . = . + 0x" +
		    Long.toHexString(Math.abs(diff)) + ";";
		if (globalVars.DEBUG)
		    System.out.println("CAS RODATA\t" + " . = . + 0x" +
				       Long.toHexString(Math.abs(diff)) +
				       ";");
		LinkerIO.addLineToLinkerScript(align_x86_64,
					       x86_offset + 1, 0);
		if (globalVars.DEBUG)
		    System.out.println("x86:::: imserted in line: " +
				       x86_offset);
	    }
	    //else do nothing: they are at the same address
	}
    }

    /*************************************************************************/
    /** set_symnol_AlignmentN                                                 */
	/*************************************************************************/
	static void set_symbol_AlignmentN(List < Tuple < String, Long, Long,
				      Long >> A_sectionList,
				      List < String > A_x86_alignment,
				      List < String > A_aarch64_alignment,
				      int size, int x86_offset,
				      int aarch64_offset,
				      boolean defaultAlign) throws
	InterruptedException, IOException {
	long difference;
	int sameAddrFlag = 0;

	for (int a = 0; a < A_sectionList.size(); a++) {
	    if (a != 0) {
		sameAddrFlag = A_sectionList.get(a - 1).getFlag();
	    }
	    //add in ALIGN rules
			/**TODO: Not sure if needed*/
	    if (a == 0) {
		//first alignment does not matter
		A_x86_alignment.add("\t" + " . = ALIGN(0x1000);");
		A_x86_alignment.add("\t*(" +
				    A_sectionList.get(a).getName() + ");");
		A_aarch64_alignment.add("\t" + " . = ALIGN(0x1000);");
		A_aarch64_alignment.add("\t*(" +
					A_sectionList.get(a).getName() +
					");");
		continue;
	    }
	    //else on later alignments, size of previous (a-1) function plays a part
	    if (A_sectionList.get(a - 1).getFlag() == 1) {
		//System.out.println("Symbol FLAGGGED:"+A_text.get(a-1).getName());
		difference =
		    A_sectionList.get(a - 2).getSize_x86_64() -
		    A_sectionList.get(a - 2).getSize_aarch64();
	    } else {
		difference =
		    A_sectionList.get(a - 1).getSize_x86_64() -
		    A_sectionList.get(a - 1).getSize_aarch64();
	    }
	    if (difference == 0) {
		//no difference align the same
		if (A_sectionList.get(a).getAlignment_x86() >
		    A_sectionList.get(a).getAlignment_aRM()) {
		    //use x86 alignment
		    A_x86_alignment.add("\t" + " . = ALIGN(0x" +
					Long.toHexString(A_sectionList.
							 get
							 (a).getAlignment_x86
							 ()) +
					" ); /*Align for" +
					A_sectionList.get(a).getName() +
					"  */");
		    A_aarch64_alignment.add("\t" + " . = ALIGN(0x" +
					    Long.
					    toHexString(A_sectionList.get
							(a).getAlignment_x86
							()) +
					    " ); /*Align for" +
					    A_sectionList.get(a).
					    getName() + "  */");
		} else {
		    //use arm alignment
		    A_x86_alignment.add("\t" + " . = ALIGN(0x" +
					Long.toHexString(A_sectionList.
							 get
							 (a).getAlignment_aRM
							 ()) +
					" ); /*Align for" +
					A_sectionList.get(a).getName() +
					"  */");
		    A_aarch64_alignment.add("\t" + " . = ALIGN(0x" +
					    Long.
					    toHexString(A_sectionList.get
							(a).getAlignment_aRM
							()) +
					    " ); /*Align for" +
					    A_sectionList.get(a).
					    getName() + "  */");
		}
		A_x86_alignment.add("\t*(" +
				    A_sectionList.get(a).getName() + ");");
		A_aarch64_alignment.add("\t*(" +
					A_sectionList.get(a).getName() +
					");");
	    } else if (difference < 0) {
		//aarch64 function version is larger than x86-64 function version
		//need to offset x86-64
		A_x86_alignment.add("\t" + " . = . + " +
				    Math.abs(difference) + ";");
		//if(A_text.get(a).getAlignment_x86() != 1 || A_text.get(a).getAlignment_aRM()!= 1){
		if (A_sectionList.get(a).getAlignment_x86() >
		    A_sectionList.get(a).getAlignment_aRM()) {
		    //use x86 alignment
		    A_x86_alignment.add("\t" + " . = ALIGN(0x" +
					Long.toHexString(A_sectionList.
							 get
							 (a).getAlignment_x86
							 ()) +
					" ); /*Align for" +
					A_sectionList.get(a).getName() +
					"  */");
		    A_aarch64_alignment.add("\t" + " . = ALIGN(0x" +
					    Long.
					    toHexString(A_sectionList.get
							(a).getAlignment_x86
							()) +
					    " ); /*Align for" +
					    A_sectionList.get(a).
					    getName() + "  */");
		} else {
		    //use arm alignment
		    A_x86_alignment.add("\t" + " . = ALIGN(0x" +
					Long.toHexString(A_sectionList.
							 get
							 (a).getAlignment_aRM
							 ()) +
					" ); /*Align for" +
					A_sectionList.get(a).getName() +
					"  */");
		    A_aarch64_alignment.add("\t" + " . = ALIGN(0x" +
					    Long.
					    toHexString(A_sectionList.get
							(a).getAlignment_aRM
							()) +
					    " ); /*Align for" +
					    A_sectionList.get(a).
					    getName() + "  */");
		}
		A_x86_alignment.add("\t*(" +
				    A_sectionList.get(a).getName() + ");");
		A_aarch64_alignment.add("\t*(" +
					A_sectionList.get(a).getName() +
					");");
	    } else {
		//aarch64 function version is smaller than x86-64 function version
		//need to offset aarch64
		if (globalVars.DEBUG)
		    System.out.
			println("Need offset aarch64 for function: " +
				A_sectionList.get(a).getName() +
				" from fn:" + A_sectionList.get(a -
								1).getName
				() + "    x86sz:0x" +
				Long.toHexString(A_sectionList.
						 get(a -
						     1).getSize_x86_64()) +
				"   aRMsz:0x" +
				Long.toHexString(A_sectionList.
						 get(a -
						     1).getSize_aarch64())
				+ "    diff: " + difference);
		A_aarch64_alignment.add("\t" + " . = . + " +
					Math.abs(difference) + ";");
		if (A_sectionList.get(a).getAlignment_x86() >
		    A_sectionList.get(a).getAlignment_aRM()) {
		    //use x86 alignment
		    A_x86_alignment.add("\t" + " . = ALIGN(0x" +
					Long.toHexString(A_sectionList.
							 get
							 (a).getAlignment_x86
							 ()) +
					" ); /*Align for" +
					A_sectionList.get(a).getName() +
					"  */");
		    A_aarch64_alignment.add("\t" + " . = ALIGN(0x" +
					    Long.
					    toHexString(A_sectionList.get
							(a).getAlignment_x86
							()) +
					    " ); /*Align for" +
					    A_sectionList.get(a).
					    getName() + "  */");
		} else {
		    //use arm alignment
		    A_x86_alignment.add("\t" + " . = ALIGN(0x" +
					Long.toHexString(A_sectionList.
							 get
							 (a).getAlignment_aRM
							 ()) +
					" ); /*Align for" +
					A_sectionList.get(a).getName() +
					"  */");
		    A_aarch64_alignment.add("\t" + " . = ALIGN(0x" +
					    Long.
					    toHexString(A_sectionList.get
							(a).getAlignment_aRM
							()) +
					    " ); /*Align for" +
					    A_sectionList.get(a).
					    getName() + "  */");
		}
		A_x86_alignment.add("\t*(" +
				    A_sectionList.get(a).getName() + ");");
		A_aarch64_alignment.add("\t*(" +
					A_sectionList.get(a).getName() +
					");");
	    }
	}			//end for A_sectionList
    }				//end set_symbol_AlignmentN

    /*************************************************************************/
	/** resetRangesInfo                                                       */
	/*************************************************************************/
	static void resetRangesInfo() {
	RangesInfo.clear();
	if (!RangesInfo.isEmpty()) {
	    System.out.println("ERROR: RANGES NOT CLEARED!!!");
	    System.exit(0);
	}
    }

	/*************************************************************************/
	/** grabSymbolsInRange                                                    */
	/*************************************************************************/
	/**  grab 
	 * @param option:
	 * 			0: x86
	 * 			1: ARM
	 * @param binary:
	 * 			exe: first run
	 * 			gold 2nd run
	 * @throws IOException: Assume nm_files have been generated | 
							Pierre: really? 
	 * @throws InterruptedException
	 */
    static void grabSymbolsInRange(int option,
				   String binary) throws IOException,
	InterruptedException {
		/** TODO: grabsymbolInRange */
	long textMin = 0, textMax = 0;
	long rodataMin = 0, rodataMax = 0;
	//long datarelrolocalMin=0, datarelrolocalMax=0;
	long dataMin = 0, dataMax = 0;
	long bssMin = 0, bssMax = 0;
	long TdataMin = 0, TdataMax = 0;
	long TbssMin = 0, TbssMax = 0;
	FileReader fr1 = null;
	String temp;

	if (option == 0) {
	    //x86
	    if (globalVars.DEBUG)
		System.out.println("\nx86 SECTION RANGES");
	    if (binary.compareTo("exe") == 0) {
		fr1 =
		    new FileReader(new
				   File(Runtime.TARGET_DIR +
					"/map_x86.txt"));
	    } else if (binary.compareTo("gold") == 0) {
		fr1 =
		    new FileReader(new
				   File(Runtime.TARGET_DIR +
					"/map_x86gold.txt"));
	    } else {
		System.
		    out.println
		    ("\n Invalid MAP file option selected for x86");
		System.exit(0);
	    }
	    //Calc Text Max Addr
	    textMin = RangesInfo.get(0).getAddr();
	    textMax =
		RangesInfo.get(0).getAddr() + RangesInfo.get(0).getSize();
	    //Calc rodata Max Addr
	    rodataMin = RangesInfo.get(1).getAddr();
	    rodataMax =
		RangesInfo.get(1).getAddr() + RangesInfo.get(1).getSize();
	    //Calc data Max Addr
	    dataMin = RangesInfo.get(2).getAddr();
	    dataMax =
		RangesInfo.get(2).getAddr() + RangesInfo.get(2).getSize();
	    //Calc bss Max Addr
	    bssMin = RangesInfo.get(3).getAddr();
	    bssMax =
		RangesInfo.get(3).getAddr() + RangesInfo.get(3).getSize();
	    //Calc tdata (TLS) Max Addr
	    TdataMin = RangesInfo.get(4).getAddr();
	    TdataMax =
		RangesInfo.get(4).getAddr() + RangesInfo.get(4).getSize();
	    //Calc tbss (TLS) Max Addr
	    TbssMin = RangesInfo.get(5).getAddr();
	    TbssMax =
		RangesInfo.get(5).getAddr() + RangesInfo.get(5).getSize();
	}			//end option 0
	else if (option == 1) {
	    //ARM
	    if (globalVars.DEBUG)
		System.out.println("\nARM SECTION RANGES\n");
	    if (binary.compareTo("exe") == 0) {
		fr1 =
		    new FileReader(new
				   File(Runtime.TARGET_DIR +
					"/map_aarch.txt"));
	    } else if (binary.compareTo("gold") == 0) {
		fr1 =
		    new FileReader(new
				   File(Runtime.TARGET_DIR +
					"/map_aarchgold.txt"));
	    } else {
		System.
		    out.println
		    ("\n Invalid MAP file option selected for ARM");
		System.exit(0);
	    }
	    //Calculate Text Max Addr
	    textMin = RangesInfo.get(6).getAddr();
	    textMax =
		RangesInfo.get(6).getAddr() + RangesInfo.get(6).getSize();
	    //Calculate rodata Max Addr
	    rodataMin = RangesInfo.get(7).getAddr();
	    rodataMax =
		RangesInfo.get(7).getAddr() + RangesInfo.get(7).getSize();
	    //Calc data Max Addr
	    dataMin = RangesInfo.get(8).getAddr();
	    dataMax =
		RangesInfo.get(8).getAddr() + RangesInfo.get(8).getSize();
	    //Calc bss Max Addr
	    bssMin = RangesInfo.get(9).getAddr();
	    bssMax =
		RangesInfo.get(9).getAddr() + RangesInfo.get(9).getSize();
	    //Calc tdata (TLS) Max Addr
	    TdataMin = RangesInfo.get(10).getAddr();
	    TdataMax =
		RangesInfo.get(10).getAddr() +
		RangesInfo.get(10).getSize();
	    //Calc tbss (TLS) Max Addr
	    TbssMin = RangesInfo.get(11).getAddr();
	    TbssMax =
		RangesInfo.get(11).getAddr() +
		RangesInfo.get(11).getSize();
	}			//end option 1

	if (globalVars.DEBUG) {
	    System.out.println("HIIII ");
	    System.out.println("Ranges text:");
	    System.out.println("0x" + Long.toHexString(textMin));
	    System.out.println("0x" + Long.toHexString(textMax));
	    System.out.println("Ranges rodata:");
	    System.out.println("0x" + Long.toHexString(rodataMin));
	    System.out.println("0x" + Long.toHexString(rodataMax));
	    System.out.println("Ranges data:");
	    System.out.println("0x" + Long.toHexString(dataMin));
	    System.out.println("0x" + Long.toHexString(dataMax));
	    System.out.println("Ranges bss:");
	    System.out.println("0x" + Long.toHexString(bssMin));
	    System.out.println("0x" + Long.toHexString(bssMax));
	    System.out.println("Ranges TLS tdata:");
	    System.out.println("0x" + Long.toHexString(TdataMin));
	    System.out.println("0x" + Long.toHexString(TdataMax));
	    System.out.println("Ranges TLS tbss:");
	    System.out.println("0x" + Long.toHexString(TbssMin));
	    System.out.println("0x" + Long.toHexString(TbssMax));
	    System.out.println("\n");
	}

	// Pierre: I want to cry
	BufferedReader br1 = new BufferedReader(fr1);
	int count = 0;
	long size = 0, addr = 0;
	String name = "", type = "";
	int BUFFERLIMIT = 160;
	int flag_foundsymbol = 0;
	long alignment = 0;
	//   ADDR            SIZE            TYPE              NAME
	//Pattern p = Pattern.compile("([0-9a-f]*)[ \t]+([0-9a-f]*)[ \t]+[wWtTdDrRbB]+[ \t]+([\\._\\-\\@A-Za-z0-9]*)");
	//NEW PATTERN FOR MAP FILE!!!!!         .section(2)  .             NAME(1)      ADDR(3)                         SIZE(4)                 ALIGNMENT(5)    SOURCE OBJECT FILE
	// Pierre: this match text|rodata|bss entries
	Pattern p =
	    Pattern.compile
	    ("^[ \\s](\\.([texrodalcbs]*)\\.[\\S]*)\\s*([x0-9a-f]*)[ \\s]*([x0-9a-f]*)[ \\s]*([0x0-9a-f]*).*");
	//conditional 2nd line pattern                  ADDR(1)                    SIZE(2)                      ALIGNMENT(3)    FILE SRC 
	Pattern pline2 =
	    Pattern.compile
	    ("^\\s*([x0-9a-f]*)[ \\s]*([x0-9a-f]*)\\s*([0x0-9a-f]*)\\s*[\\/\\w\\(\\)\\.\\-]*");
	//Pattern p_x86CodeFill = Pattern.compile("^[ \\s]\\*\\*[ \\s]fill\\s*([x0-9a-f]*)[ \\s]*([x0-9a-f]*)");
	//Pattern p_aarchCodeFill = Pattern.compile("^\\s\\*\\*\\szero fill\\s*([x0-9a-f]*)[ \\s]+([x0-9a-f]*)");
	while ((temp = br1.readLine()) != null) {
	    br1.mark(BUFFERLIMIT);
	    ++count;

	    Matcher m = p.matcher(temp);
	    if (m.find()) {
		if ((!m.group(1).isEmpty()) && m.group(3).isEmpty()
		    && m.group(4).isEmpty()) { // PIERRE: match pattern 1 type 1
		    /*if(m.group(2).compareTo("tbss")==0){
		       System.out.println(">>>>>>>>>>>What regex finds: m1:"+m.group(1)+"#    m2:"+m.group(2)+"#    m3:"+m.group(3)+"#    m4:"+m.group(4));
		       } */
		    //means we probably hit a long symbol name and important stuff is on the next line
		    //save the name at least

		    name = m.group(1);
		    type = m.group(2);
		    //System.out.println("BSS OLD temp: "+name);
		    if (name.indexOf(".", 8) > 7) {
			if (type.compareTo("rodata") == 0) {
			    //System.out.println("RODATA DOTY FOUND: "+name);
			    name =
				name.substring(0, name.lastIndexOf("."));
			    name = name.concat(".*");
			    System.out.println("FIXED RODATA DOT: " +
					       name);
			} else if (name.
				   compareTo(".text..omp_outlined.") == 0
				   ||
				   name.compareTo
				   (".text..omp.reduction.reduction_func")
				   == 0) {
			    //System.out.println("MOTEEEEEE BAD OMP");
			    continue;
			} else {
			    String dot =
				name.substring(0, name.lastIndexOf("."));
			    if (dot.compareTo(".text..omp_outlined.") == 0
				||
				dot.compareTo
				(".text..omp.reduction.reduction_func")
				== 0) {
				//skip these symbols
			    } else {
				//transform
				//System.out.println("NOT OMP outlined: "+dot);
				name = dot;
				name = name.concat(".*");
				System.out.println("new: " + name);
			    }
			}	//end ELSE
		    }		//end DOT NAME


		    //read and utilize the next line
		    temp = br1.readLine();
		    br1.mark(BUFFERLIMIT);
		    Matcher m2 = pline2.matcher(temp);
		    while (m2.find()) {
			//System.out.println("regex line2 finds: m1:"+m2.group(1)+"#    m2:"+m2.group(2));

			if (m2.group(2).isEmpty()) {
			    size = 0;
			} else {
			    size =
				Long.parseLong(m2.
					       group(2).replaceFirst("0x",
								     ""),
					       16);
			}
			addr =
			    Long.parseLong(m2.group(1).
					   replaceFirst("0x", ""), 16);
			if (!m2.group(3).isEmpty()) {
			    alignment =
				Long.parseLong(m2.
					       group(3).replaceFirst("0x",
								     ""),
					       16);
			    //System.out.println(name+"ALIGNMENT:"+m2.group(3));
			}
			if (m.group(2).compareTo("tbss") == 0) {
			    if (globalVars.DEBUG)
				System.out.
				    println("#########LONG SYMBOL NAME: " +
					    name + "    addr:" + addr +
					    "    size:0x" + m2.group(2));
			}
			flag_foundsymbol = 1;
		    }
		}		//end if evil format // Pierre: match pattern1 type 2
		else if (!m.group(1).isEmpty() && !m.group(3).isEmpty()
			 && Long.parseLong(m.group(4).
					   replaceFirst("0x", ""),
					   16) != 0) {
		    //System.out.println("A: "+m.group(1)+"\tb: "+m.group(2)+"\tC: "+m.group(3));
		    name = m.group(1);
		    if (m.group(4).isEmpty()) {
			size = 0;
		    } else {
			size =
			    Long.parseLong(m.group(4).
					   replaceFirst("0x", ""), 16);
		    }
		    if (!m.group(5).isEmpty()) {
			alignment =
			    Long.parseLong(m.group(5).
					   replaceFirst("0x", ""), 16);
			//System.out.println(name+"ALIGNMENT:"+m.group(5));
		    }
		    addr =
			Long.parseLong(m.group(3).replaceFirst("0x", ""),
				       16);
		    /*if(name.compareTo(".text.main")==0 ||name.compareTo(".text.randlc")==0||name.compareTo(".text.rank")==0)
		       System.out.println("normal SYMBOL NAME: "+name+"    addr:0x"+Long.toHexString(addr)+"    size:0x"+Long.toHexString(size));
		     */
		    flag_foundsymbol = 1;
		}		//end if nice format
	    }			//end while m.find()

	    if (flag_foundsymbol == 1) {

		//Plog.log("symbol found: name=" + name + ", addr=" + addr + ", size=" +
		//	size + ", alignment=" + alignment + "\n");

		flag_foundsymbol = 0;
		//if(type.compareTo("text")==0 && option==0){
		//      System.out.println("real b4 populate sections: "+ name+"     addr:0x"+Long.toHexString(addr)+"      size:0x"+Long.toHexString(size));
		//}
				/**NEW STREAMLINED INTO 1 function**/
		populateSections("text", option, globalVars.A_text, name,
				 addr, size, textMin, textMax, alignment,
				 count);
		populateSections("rodata", option, globalVars.A_rodata,
				 name, addr, size, rodataMin, rodataMax,
				 alignment, count);
		//populateSections("data_Relrolocal", option, globalVars.A_data_Relrolocal, name, addr, size, datarelrolocalMin, datarelrolocalMax,alignment,count);
		populateSections("data", option, globalVars.A_data, name,
				 addr, size, dataMin, dataMax, alignment,
				 count);
		populateSections("bss", option, globalVars.A_bss, name,
				 addr, size, bssMin, bssMax, alignment,
				 count);
		if (globalVars.hasTLS_DATA)
		    populateSections("tdata", option,
				     globalVars.A_data_TLS, name, addr,
				     size, TdataMin, TdataMax, alignment,
				     count);
		if (globalVars.hasTLS_BSS)
		    populateSections("tbss", option, globalVars.A_bss_TLS,
				     name, addr, size, TbssMin, TbssMax,
				     alignment, count);
	    }			//end if flag_foundsymbol
	}			//end while readline
	}

	/*************************************************************************/
	/** populateSections                                                      */
	/*************************************************************************/
    static void populateSections(String region, int option,
				 List < Tuple < String, Long, Long,
				 Long >> currList, String name, long addr,
				 long size, long sectionMin,
				 long sectionMax, long alignment,
				 int count) throws IOException,
	InterruptedException {
	long default_size = 0;
	long init_align = 1;
	int c = 0;

	if (addr < sectionMax && addr >= sectionMin) {
	    //System.out.println("WARNING: "+sectionMin+"  "+sectionMax+"   adr:"+addr);
	    //check if the symbol already exists
	    for (int t = 0; t < currList.size(); t++) {
		if (currList.get(t).getName().compareTo(name) == 0) {
		    if (option == 0) {
			//add to size x86
			currList.get(t).updateBy1_MultAddressFlag();
			if (currList.get(t).getMultAddressFlag() > 1) {
			    if (globalVars.DEBUG)
				System.out.println("x86$$$$$$$$$ " +
						   currList.get(t).
						   getName() +
						   " MULTIPLE ADDRESS!!! "
						   +
						   currList.
						   get
						   (t).getMultAddressFlag
						   ());
			    //same symbol section different address
			    //NEED TO SIMULATE ARCH RELATIVE ALIGNMENT
			    long simulatedAlignmentPadding =
				alignCustom_getVal(currList.
						   get(t).getSize_x86_64(),
						   currList.
						   get(t).getAlignment_x86
						   ());
			    if ((currList.get(t).getSize_x86_64() +
				 size) %
				currList.get(t).getAlignment_x86() == 0) {
				//dont need simulatedd padding
				currList.get(t).setSize_x86_64(size +
							       currList.
							       get
							       (t).getSize_x86_64
							       ());
				System.
				    out.println
				    ("%%% DONT NEED MultAddressFlag>1 for "
				     + currList.get(t).getName() +
				     "\tPAD:0x" +
				     Long.toHexString
				     (simulatedAlignmentPadding)
				     + "\tNew Size:0x" +
				     Long.toHexString(currList.
						      get(t).getSize_x86_64
						      ()));
			    } else {
				//need simulated padding
				currList.
				    get(t).setSize_x86_64
				    (simulatedAlignmentPadding + size +
				     currList.get(t).getSize_x86_64());
				System.
				    out.println
				    ("%%% NEED MultAddressFlag>1 for " +
				     currList.get(t).getName() +
				     "\tPAD:0x" +
				     Long.toHexString
				     (simulatedAlignmentPadding)
				     + "\tNew Size:0x" +
				     Long.toHexString(currList.
						      get(t).getSize_x86_64
						      ()));
			    }

			} else {
			    currList.get(t).setSize_x86_64(currList.
							   get
							   (t).getSize_x86_64
							   () + size);

			    if (alignment >
				currList.get(t).getAlignment_x86()) {
				if ((alignment %
				     currList.get(t).getAlignment_x86()) !=
				    0) {
				    System.
					out.println
					("ERROR:\t ALIGNMENTS ARE \bNOT MULTIPLES OF THEMSELVES");
				    System.exit(0);
				}
				currList.get(t).
				    setAlignment_x86(alignment);
			    }
			}	//end else if(getMultAddressFlag()>1)
			//currList.get(t).setAlignment_x86(alignment);
		    }
		    if (option == 1) {
			//add to size arm
			currList.get(t).updateBy1_MultAddressFlag_ARM();
			if (currList.get(t).getMultAddressFlag_ARM() > 1) {
			    if (globalVars.DEBUG)
				System.out.println("ARM$$$$$$$$$ " +
						   currList.get(t).
						   getName() +
						   " MULTIPLE ADDRESS!!! "
						   +
						   currList.
						   get
						   (t).getMultAddressFlag
						   ());
			    //same symbol section different address
			    //NEED TO SIMULATE ARCH RELATIVE ALIGNMENT
			    long simulatedAlignmentPadding =
				alignCustom_getVal(currList.
						   get(t).getSize_aarch64
						   (),
						   currList.
						   get(t).getAlignment_aRM
						   ());
			    if ((currList.get(t).getSize_aarch64() +
				 size) %
				currList.get(t).getAlignment_aRM() == 0) {
				//dont need simulatedd padding
				currList.get(t).setSize_aarch64(size +
								currList.get
								(t).getSize_aarch64
								());
				//System.out.println("%%% DONT NEED MultAddressFlag>1 for "+currList.get(t).getName()+"\tPAD:0x"+Long.toHexString(simulatedAlignmentPadding)+"\tNew Size:0x"+Long.toHexString(currList.get(t).getSize_aarch64()));                                      
			    } else {
				//need simulated padding
				currList.
				    get(t).setSize_aarch64
				    (simulatedAlignmentPadding + size +
				     currList.get(t).getSize_aarch64());
				//System.out.println("%%% NEED MultAddressFlag>1 for "+currList.get(t).getName()+"\tPAD:0x"+Long.toHexString(simulatedAlignmentPadding)+"\tNew Size:0x"+Long.toHexString(currList.get(t).getSize_aarch64()));
			    }
			} else {
			    currList.get(t).setSize_aarch64(currList.
							    get
							    (t).getSize_aarch64
							    () + size);
			    System.out.println("ARM UPDATE " +
					       currList.get(t).getName() +
					       "\tNew Size:0x" +
					       Long.
					       toHexString(currList.get
							   (t).getSize_aarch64
							   ()));
			    if (alignment >
				currList.get(t).getAlignment_aRM()) {
				if ((alignment %
				     currList.get(t).getAlignment_aRM()) !=
				    0) {
				    System.
					out.println
					("ERROR:\t ALIGNMENTS ARE \bNOT MULTIPLES OF THEMSELVES");
				    System.exit(0);
				}
				currList.get(t).
				    setAlignment_aRM(alignment);
			    }
			}	//end else if(getMultAddressFlag()>1)
			//currList.get(t).setAlignment_aRM(alignment);
		    }
		    c = 1;
		    break;
		}		//end if symbol name match
	    }			//end for all A_text

	    if (c != 1) {
		//Add NEW SYMBOL!!
		if (option == 0) {
		    if (globalVars.DEBUG)
			System.out.println(" " + region + " NEW X86 Sym:" +
					   name + "     addr:0x" +
					   Long.toHexString(addr) +
					   "      size:0x" +
					   Long.toHexString(size));
		    checkSameAddr_MultipleSymbol(currList, addr);
		    if (currList.size() != 0
			&& (currList.get(currList.size() - 1).getFlag() ==
			    1)) {
			//same addr multipleSymbol is true!
			//A_text.add(new Tuple<String,Long,Long,Long>( m.group(3), size ,(long)0,addr ) );
			if (globalVars.DEBUG)
			    System.out.println(region + "   NEW x86:" +
					       name + "  sz: 0x" +
					       Long.toHexString(size));
			currList.add(new Tuple < String, Long, Long,
				     Long > (name, size, default_size,
					     addr, alignment, init_align));
		    } else {
			currList.add(new Tuple < String, Long, Long,
				     Long > (name, size, default_size,
					     addr, alignment, init_align));
		    }
		} else if (option == 1) {
		    if (globalVars.DEBUG)
			System.out.println("NEW ARM Sym:" + name +
					   "     addr:0x" +
					   Long.toHexString(addr) +
					   "      size:0x" +
					   Long.toHexString(size));
		    currList.add(new Tuple < String, Long, Long,
				 Long > (name, default_size, size, addr,
					 init_align, alignment));
		}		//end if option
		c = 0;
	    }			//end if c
	}			//end if text range
    }

	/*************************************************************************/
	/** alignCustom_getVal                                                    */
	/*************************************************************************/
    static Long alignCustom_getVal(Long input, Long mod) {
	long temp = 0;
	while (input % mod != 0) {
	    input += 1;
	    temp += 1;
	}
	return temp;
    }

	/**************************************************************************/
	/** AntonioOffsetAdditional                                                */
	/**************************************************************************/
	/**TODO: Antonio function
	 * @throws IOException 
	 * Should make more generic to cascade for N additional architecture linker scripts
	 */
    static long AntonioOffsetAdditional(String section) throws IOException {
	String temp;
	String name = null;
	long size = 0;
	long totalsneakyoffset = 0;
	int x86_insertPT, arm_insertPT;

	switch (section) {
	case "text":
	    x86_insertPT =
		LinkerIO.x86_textLineOffset +
		LinkerIO.getLinkerNewLines_x86() + 1;
	    arm_insertPT =
		LinkerIO.aarch64_textLineOffset +
		LinkerIO.getLinkerNewLines_aRM() + 1;
	    break;
	    case "rodata":x86_insertPT =
		LinkerIO.x86_rodataLineOffset +
		LinkerIO.getLinkerNewLines_x86() + 1;
	    arm_insertPT =
		LinkerIO.aarch64_rodataLineOffset +
		LinkerIO.getLinkerNewLines_aRM() + 1;
	    break;
	    case "data":x86_insertPT =
		LinkerIO.x86_dataLineOffset +
		LinkerIO.getLinkerNewLines_x86() + 1;
	    arm_insertPT =
		LinkerIO.aarch64_dataLineOffset +
		LinkerIO.getLinkerNewLines_aRM() + 1;
	    break;
	    case "bss":x86_insertPT =
		LinkerIO.x86_bssLineOffset +
		LinkerIO.getLinkerNewLines_x86() + 1;
	    arm_insertPT =
		LinkerIO.aarch64_bssLineOffset +
		LinkerIO.getLinkerNewLines_aRM() + 1;
	    break;
	    case "tdata":x86_insertPT =
		LinkerIO.x86_tlsdataLineOffset +
		LinkerIO.getLinkerNewLines_x86() + 1;
	    arm_insertPT =
		LinkerIO.aarch64_tlsdataLineOffset +
		LinkerIO.getLinkerNewLines_aRM() + 1;
	    break;
	    case "tbss":x86_insertPT =
		LinkerIO.x86_tlsbssLineOffset +
		LinkerIO.getLinkerNewLines_x86() + 1;
	    arm_insertPT =
		LinkerIO.aarch64_tlsbssLineOffset +
		LinkerIO.getLinkerNewLines_aRM() + 1;
	    break;
	    default:throw new IllegalArgumentException("Invalid Section: "
						       + section);
	}
	//SHADDOW PATTERN FOR MAP FILE!!!!!             .section(1)                 ADDR(2)                              SIZE(3)                        ALIGNMENT(4)    SOURCE OBJECT FILE
	    Pattern p =
	    Pattern.compile
	    ("^[ \\s]\\.([texrodalcbs]*)\\s*([x0-9a-f]*)[ \\s]*(0x[0-9a-f]*)[ \\s]*([0x0-9a-f]*).*");
	//2nd line pattern                                              ADDR(1)                 SYMBOL(2) 
	Pattern pline2 =
	    Pattern.compile("^\\s*([x0-9a-f]*)\\s+([_0-9a-z]*)\\s*");

	//X86 
	FileReader fr1 =
	    new FileReader(new
			   File(Runtime.TARGET_DIR + "/map_x86gold.txt"));
	BufferedReader br1 = new BufferedReader(fr1);
	while ((temp = br1.readLine()) != null) {
	    Matcher m = p.matcher(temp);
	    if (m.find()) {
		if (m.group(1).compareTo(section) == 0
		    && m.group(3).compareTo("0x0") != 0) {
		    //matches section and is not size 0x0
		    size =
			Long.parseLong(m.group(3).replaceFirst("0x", ""),
				       16);
		    totalsneakyoffset += size;

		    //read and utilize the next line
		    temp = br1.readLine();
		    Matcher m2 = pline2.matcher(temp);
		    while (m2.find()) {
			//System.out.println("regex line2 finds: m1:"+m2.group(1)+"#    m2:"+m2.group(2));
			name = m2.group(2);
		    }
		    if (globalVars.DEBUG)
			System.out.println("found sneaky: " + name +
					   "  addr:" + m.group(2) +
					   "  sz:" + m.group(3));
		}
	    }
	}			//end while m.find()
	fr1.close();
	br1.close();
	if (globalVars.DEBUG)
	    System.out.println("Total shadow symbol size for x86:0x" +
			       Long.toHexString(totalsneakyoffset));
	//Add this much to "next" architecture linker script
	LinkerIO.sectionSize_alignment.add("\t" + " . = . + 0x" +
					   Long.
					   toHexString(totalsneakyoffset) +
					   "; /*ANTONIO SNEAKY " +
					   section +
					   " NonOverlapping Offset*/");
	LinkerIO.addAlignmentToLinkerScripts(null,
					     LinkerIO.
					     sectionSize_alignment, 0,
					     arm_insertPT);

	//Keep adding for each architecture
	/*
	   //ARM
	   FileReader fr2 = new FileReader(new File(Runtime.TARGET_DIR+"/map_aarchgold.txt"));
	   BufferedReader br2 = new BufferedReader(fr2);
	   while((temp = br1.readLine()) != null){
	   Matcher m = p.matcher(temp);
	   if(m.find()){
	   if( m.group(1).compareTo(section)==0 && m.group(3).compareTo("0x0")!=0 ){
	   //matches section and is not size 0x0
	   size = Long.parseLong(m.group(3).replaceFirst("0x", ""),16);
	   size +=totalsneakyoffset;

	   //read and utilize the next line
	   temp = br1.readLine();
	   Matcher m2= pline2.matcher(temp);
	   while(m2.find()){
	   //System.out.println("regex line2 finds: m1:"+m2.group(1)+"#    m2:"+m2.group(2));
	   name = m2.group(2);
	   }    

	   System.out.println("found sneaky: "+name+"  addr:"+m.group(2)+"  sz:"+m.group(3));
	   }
	   }
	   }//end while m.find()
	   fr2.close();
	   br2.close();
	 */
	LinkerIO.sectionSize_alignment.clear();
	return totalsneakyoffset;
    }

	/*************************************************************************/
	/** anomolyARMSizeChecker()                                              */
	/*************************************************************************/
    static void anomolyARMSizeChecker() throws InterruptedException,
	IOException {
	//Read in map file x86
	String temp;
	//List<Tuple<String,Long,Long,Long>> tempAr = new ArrayList<Tuple<String,Long,Long,Long>>();
	 List < String > tempAr = new ArrayList < String > ();

	//open the text file
	FileReader fr1 =
	    new FileReader(new
			   File(Runtime.TARGET_DIR + "/map_aarch.txt"));
	BufferedReader br1 = new BufferedReader(fr1);

	//read into array
	while ((temp = br1.readLine()) != null) {
	    tempAr.add(temp);	//new Tuple<String,Long,Long,Long>(temp,(long)0,(long)0,(long)0));
	} br1.close();

	/*for(int a = 910 ; a < 930 ; a++){
	   System.out.println("ANONOMLY:"+tempAr.get(a)+"QQEND");
	   } */

	//define pattern we are looking for:     .text.    |SYMBOLNAME    | \n      |  ADDR   | space |  SIZE    | space | PATH TO OBJ FILE
	Pattern p1 =
	    Pattern.compile
	    ("[ \\s]+\\.[texdarobs]*\\.([0-9a-z_\\.\\-]*)[ \\s]*[x0-9a-f]*[ \\s]*0?x?([0-9a-f]*)");
	Pattern p2 =
	    Pattern.compile
	    ("[ \\s]+[x0-9a-f]+[ \\s]+0x([0-9a-f]*)[ \\s]+[a-zA-Z_/.0-9()\\-]*");
	Matcher m, m1;
	int wflag = 0;

	for (int a = 0; a < tempAr.size() - 1; a++) {
	    m = p1.matcher(tempAr.get(a));
	    //if(m.group(1).compareTo("_svfprintf_r")==0){
	    while (m.find()) {
		//System.out.println("ANONOMLY: "+m.group());
		if (!m.group(2).isEmpty()) {
		    //System.out.println("ANONOMLY: "+m.group());
		    for (int b = 0; b < globalVars.A_text.size(); b++) {
			if (m.
			    group(1).compareTo(globalVars.A_text.get(b).
					       getName()) == 0) {
			    //if(!m1.group(1).isEmpty()){
			    for (int w = 0;
				 w < globalVars.Antonio_WhiteList.size();
				 w++) {
				if (globalVars.A_text.get(b).
				    getName().compareTo(globalVars.
							Antonio_WhiteList.get
							(w)) == 0) {
				    wflag = 1;
				    //System.out.println("WHITELIST MATCH:"+Antonio_WhiteList.get(w));
				    break;
				}
			    }
			    if (wflag != 1) {
				if (globalVars.A_text.get(b).
				    getSize_aarch64() <
				    Long.parseLong(m.group(2), 16)) {
				    System.out.
					println("SIZE text ANOMOLY!: " +
						m.group(1) +
						"Orig Sz: 0x" +
						Long.toHexString
						(globalVars.A_text.get
						 (b).getSize_aarch64()) +
						"  MapSz: 0x" +
						m.group(2));
				    globalVars.A_text.
					get(b).setSize_aarch64
					(Long.parseLong(m.group(2), 16));
				}
			    }
			    wflag = 0;
			    //}
			}
		    }
		    for (int b = 0; b < globalVars.A_rodata.size(); b++) {
			if (m.
			    group(1).compareTo(globalVars.A_rodata.get(b).
					       getName()) == 0) {
			    if (!m.group(2).isEmpty())
				if (globalVars.A_rodata.get(b).
				    getSize_aarch64() <
				    Long.parseLong(m.group(2), 16)) {
				    System.out.
					println("SIZE text ANOMOLY!: " +
						m.group(1) +
						"  \tOrig Sz: 0x" +
						Long.toHexString
						(globalVars.A_rodata.get
						 (b).getSize_aarch64()) +
						"\tMapSz: 0x" +
						m.group(2));
				    globalVars.A_rodata.
					get(b).setSize_aarch64
					(Long.parseLong(m.group(2), 16));
				}
			}
		    }
		    for (int b = 0; b < globalVars.A_data.size(); b++) {
			if (m.
			    group(1).compareTo(globalVars.A_data.get(b).
					       getName()) == 0) {
			    if (!m.group(2).isEmpty())
				if (globalVars.A_data.get(b).
				    getSize_aarch64() <
				    Long.parseLong(m.group(2), 16)) {
				    System.out.
					println("SIZE text ANOMOLY!: " +
						m.group(1) +
						"\\tOrig Sz: 0x" +
						Long.toHexString
						(globalVars.A_data.
						 get(b).getSize_aarch64())
						+ "\\tMapSz: 0x" +
						m.group(2));
				    globalVars.A_data.get(b).
					setSize_aarch64(Long.parseLong
							(m.group(2), 16));
				}
			}
		    }
		    for (int b = 0; b < globalVars.A_bss.size(); b++) {
			if (m.
			    group(1).compareTo(globalVars.A_bss.get(b).
					       getName()) == 0) {
			    if (!m.group(2).isEmpty())
				if (globalVars.A_bss.get(b).
				    getSize_aarch64() <
				    Long.parseLong(m.group(2), 16)) {
				    System.out.
					println("SIZE text ANOMOLY!: " +
						m.group(1) +
						"Orig Sz: 0x" +
						Long.toHexString
						(globalVars.A_bss.
						 get(b).getSize_aarch64())
						+ "  MapSz: 0x" +
						m.group(2));
				    globalVars.A_bss.get(b).
					setSize_aarch64(Long.parseLong
							(m.group(2), 16));
				}
			}
		    }
		    //end for loops
		} else {
		    m1 = p2.matcher(tempAr.get(a + 1));
		    while (m1.find()) {
			//this line contains the size so compare it to whats already there for nm
			for (int b = 0; b < globalVars.A_text.size(); b++) {
			    if (m.
				group(1).compareTo(globalVars.A_text.
						   get(b).getName()) ==
				0) {
				if (!m1.group(1).isEmpty()) {
				    for (int w = 0;
					 w <
					 globalVars.Antonio_WhiteList.
					 size(); w++) {
					if (globalVars.A_text.get(b).
					    getName().compareTo
					    (globalVars.Antonio_WhiteList.get
					     (w)) == 0) {
					    wflag = 1;
					    System.
						out.println
						("WHITELIST MATCH:" +
						 globalVars.Antonio_WhiteList.
						 get(w));
					    break;
					}
				    }
				    if (wflag != 1) {
					if (globalVars.A_text.
					    get(b).getSize_aarch64() <
					    Long.parseLong(m1.group(1),
							   16)) {
					    System.
						out.println
						("SIZE text ANOMOLY!: " +
						 m.group(1) +
						 "Orig Sz: 0x" +
						 Long.toHexString
						 (globalVars.A_text.get
						  (b).getSize_aarch64()) +
						 "  MapSz: 0x" +
						 m1.group(1));
					    globalVars.A_text.
						get(b).setSize_aarch64
						(Long.parseLong
						 (m1.group(1), 16));
					}
				    }
				    wflag = 0;
				}
			    }
			}
			for (int b = 0; b < globalVars.A_rodata.size();
			     b++) {
			    if (m.
				group(1).compareTo(globalVars.A_rodata.
						   get(b).getName()) ==
				0) {
				if (!m1.group(1).isEmpty())
				    if (globalVars.A_rodata.
					get(b).getSize_aarch64() <
					Long.parseLong(m1.group(1), 16)) {
					System.out.
					    println("SIZE text ANOMOLY!: "
						    + m.group(1) +
						    "  \tOrig Sz: 0x" +
						    Long.toHexString
						    (globalVars.A_rodata.get
						     (b).getSize_aarch64())
						    + "\tMapSz: 0x" +
						    m1.group(1));
					globalVars.A_rodata.
					    get(b).setSize_aarch64
					    (Long.parseLong
					     (m1.group(1), 16));
				    }
			    }
			}
			for (int b = 0; b < globalVars.A_data.size(); b++) {
			    if (m.
				group(1).compareTo(globalVars.A_data.
						   get(b).getName()) ==
				0) {
				if (!m1.group(1).isEmpty())
				    if (globalVars.A_data.
					get(b).getSize_aarch64() <
					Long.parseLong(m1.group(1), 16)) {
					System.out.
					    println("SIZE text ANOMOLY!: "
						    + m.group(1) +
						    "\\tOrig Sz: 0x" +
						    Long.toHexString
						    (globalVars.A_data.get
						     (b).getSize_aarch64())
						    + "\\tMapSz: 0x" +
						    m1.group(1));
					globalVars.A_data.
					    get(b).setSize_aarch64
					    (Long.parseLong
					     (m1.group(1), 16));
				    }
			    }
			}
			for (int b = 0; b < globalVars.A_bss.size(); b++) {
			    if (m.
				group(1).compareTo(globalVars.A_bss.get(b).
						   getName()) == 0) {
				if (!m1.group(1).isEmpty())
				    if (globalVars.A_bss.
					get(b).getSize_aarch64() <
					Long.parseLong(m1.group(1), 16)) {
					System.out.
					    println("SIZE text ANOMOLY!: "
						    + m.group(1) +
						    "Orig Sz: 0x" +
						    Long.toHexString
						    (globalVars.A_bss.get
						     (b).getSize_aarch64())
						    + "  MapSz: 0x" +
						    m1.group(1));
					globalVars.A_bss.
					    get(b).setSize_aarch64
					    (Long.parseLong
					     (m1.group(1), 16));
				    }
			    }
			}
			//end for loops
		    }		//end while m1
		}		//end else m.group(2) not empty
	    }			//end while m
	}			//end for loop

	//Read in map file
	//PATTERN
	// [ \\t]+\.text\.NAMEOFSYMBOL\\n
	// [ \\t]+[0-9xa-fA-F]*[ \\t]+([0-9xa-fA-F]*)[ \\t]+[a-zA-Z\\-/_\\.\\(\\)]*\\n
	// [ \\t]+[0-9xa-fA-F]*[ \\t]+([a-zA-Z_\\-\\.0-9]*)

	//if nm size doesnt match vanilla size use vanilla binary size to readjust
    }
}
	/**************************************************************************/
	/**************************************************************************/
	/* Pierre: everything below is commented */
	/**************************************************************************/
	/**************************************************************************/

/**
 * @param option
 * @throws IOException:  map_x86.txt, map_aarch.txt
 * @throws InterruptedException
 * Options:
 * 		0: x86
 * 		1: ARM
 */
/*	static void grabSymbolsInRange_SCOUT(int option) throws IOException, InterruptedException{
	long textMin=0,textMax=0;
	long rodataMin=0,rodataMax=0;
	long dataMin=0,dataMax=0;
	long bssMin=0,bssMax=0;
	FileReader fr1 =null;
	String temp;
	if(option == 0){
		//x86
		//System.out.println("\nx86 SECTION RANGES_S");
		fr1 = new FileReader(new File(Runtime.TARGET_DIR+"/map_x86.txt"));
		
		//Calculate Text Max Addr
		textMin = RangesInfo.get(0).getAddr();
		textMax = RangesInfo.get(0).getAddr() + RangesInfo.get(0).getSize();
		//Calculate rodata Max Addr
		rodataMin =RangesInfo.get(1).getAddr();
		rodataMax = RangesInfo.get(1).getAddr() + RangesInfo.get(1).getSize();
		//Calc data Max Addr
		dataMin =RangesInfo.get(2).getAddr();
		dataMax = RangesInfo.get(2).getAddr() + RangesInfo.get(2).getSize();
		//Calc bss Max Addr
		bssMin =RangesInfo.get(3).getAddr();
		bssMax = RangesInfo.get(3).getAddr() + RangesInfo.get(3).getSize();
		//Calc data.rel.ro.local Max Addr
		//datarelrolocalMin =RangesInfo.get(2).getSize_x86_64();
		//datarelrolocalMax = RangesInfo.get(2).getSize_x86_64() + RangesInfo.get(2).getSize_aarch64();
		
	}//end option 0
	else if(option == 1){
		//arm
		//System.out.println("\nARM SECTION RANGES_S\n");
		fr1 = new FileReader(new File(Runtime.TARGET_DIR+"/map_aarch.txt"));
		
		//Calculate Text Max Addr
		textMin =RangesInfo.get(4).getAddr();
		textMax = RangesInfo.get(4).getAddr() + RangesInfo.get(4).getSize();
		//Calculate rodata Max Addr
		rodataMin =RangesInfo.get(5).getAddr();
		rodataMax = RangesInfo.get(5).getAddr() + RangesInfo.get(5).getSize();
		//Calc data Max Addr
		dataMin =RangesInfo.get(6).getAddr();
		dataMax = RangesInfo.get(6).getAddr() + RangesInfo.get(6).getSize();
		//Calc bss Max Addr
		bssMin =RangesInfo.get(7).getAddr();
		bssMax = RangesInfo.get(7).getAddr() + RangesInfo.get(7).getSize();
		//Calc data.rel.ro Max Addr
		//datarelrolocalMin =RangesInfo.get(7).getSize_x86_64();
		//datarelrolocalMax = RangesInfo.get(7).getSize_x86_64() + RangesInfo.get(7).getSize_aarch64();
	}//end option 1
	
	BufferedReader br1 = new BufferedReader(fr1);
	int count =0;
	long size=0,addr=0;
	String name="";
	int flag_foundsymbol=0;
							//				SECTION(2)    FULLNAME(1)		ADDR(3)			SIZE(4)			ALIGNMENT(5) SYMBOL_SRC
	Pattern p = Pattern.compile("^[ \\s](\\.([texrodalcbs]*)\\.[\\S]*)\\s*([x0-9a-f]*)[ \\s]*([x0-9a-f]*)[ \\s]*([0x0-9a-f]*).*");
							//				
	Pattern pline2 = Pattern.compile("^\\s*([x0-9a-f]*)[ \\s]*([x0-9a-f]*)\\s*([0x0-9a-f]*)\\s*[\\/\\w\\(\\)\\.\\-]*");
	while((temp = br1.readLine()) != null){
		Matcher m = p.matcher(temp);
		if(m.find()){
			//System.out.println("What regex finds: m1:"+m.group(1)+"#    m2:"+m.group(3)+"#    m3:"+m.group(4)+"#    align:"+m.group(5));
			if( (!m.group(1).isEmpty()) && m.group(3).isEmpty() && m.group(4).isEmpty()){
				//means we probably hit a long symbol name and important stuff is on the next line
				//save the name at least
				//System.out.println("EVIL FORMAT m1: "+m.group(1)+"\t\tm2: "+m.group(2));
				name = m.group(1);
				//read and utilize the next line
				temp = br1.readLine();
				Matcher m2= pline2.matcher(temp);
				while(m2.find()){
					addr = Long.parseLong(m2.group(1).replaceFirst("0x", ""),16);
					size = Long.parseLong(m2.group(2).replaceFirst("0x", ""),16);
					flag_foundsymbol =1;
				}
				
			}//end if evil format
			else if(!m.group(1).isEmpty()  && !m.group(3).isEmpty() && Long.parseLong(m.group(4).replaceFirst("0x", ""),16)!=0 && !m.group(5).isEmpty() ){
				//System.out.println("NICE FORMAT m1: "+m.group(1)+"\t\tm2: "+m.group(2)+"\tm3: "+m.group(3));
				name = m.group(1);
				size = Long.parseLong(m.group(4).replaceFirst("0x", ""),16);
				addr = Long.parseLong(m.group(3).replaceFirst("0x", ""),16);
				flag_foundsymbol =1;
			}//end if nice format
		}//end while m.find()
		
		if(flag_foundsymbol==1){
			flag_foundsymbol=0;
			populateSections_SCOUT("text", option, globalVars.A_text, name, addr, size,textMin, textMax,count);
			populateSections_SCOUT("rodata", option, globalVars.A_rodata, name, addr, size, rodataMin, rodataMax,count);
			populateSections_SCOUT("data", option, globalVars.A_data, name, addr, size, dataMin, dataMax,count);
			populateSections_SCOUT("bss", option, globalVars.A_bss, name, addr, size, bssMin, bssMax,count);
		}//end if flag_foundsymbol
	}//end while readline
	//align_aarch64_MultipleSymbolDiffAddr(A_text);
}//END grabSymbolsInRange_SCOUT

static void populateSections_SCOUT(String region, int option, List<Tuple<String,Long,Long,Long>> currList, String name, long addr, long size, long sectionMin, long sectionMax,int count) throws IOException, InterruptedException{
	long default_size = 0;
	long init_align= 1;

	//System.out.println("in populateSections m1: "+name+ "       m2:"+m2+"       m3:"+m3);
	if(addr <= sectionMax && addr >= sectionMin){
		//check if the symbol already exists
		for(int t=0;t<currList.size();t++){
			if(currList.get(t).getName().compareTo(name)==0){
				if(option==0){
					currList.get(t).setSize_x86_64(size);
				}else if(option==1){
					currList.get(t).setSize_aarch64(size);
				}
				return;
			}//end if symbol name match
		}//end for all currlist
		
		//Add NEW SYMBOL!!
		if(option==0){
			//checkSameAddr_MultipleSymbol(currList, addr);
			if(currList.size()!=0 && (currList.get(currList.size()-1).getFlag()==1) ){
				//same addr multipleSymbol is true!
				System.out.println("SDFKSFABWEFJBWEF FLAG ERROR");
				currList.add(new Tuple<String,Long,Long,Long>( name, size, default_size, addr, init_align, init_align) );					
			}else{
				//System.out.println(region+"   NEW x86:"+name+"  sz: 0x"+Long.toHexString(size));
				currList.add(new Tuple<String,Long,Long,Long>( name, size ,default_size, addr, init_align, init_align) );
			}
		}else if(option==1){
			//System.out.println(region+" NEW ARM:"+name+"  sz:"+size);
			currList.add(new Tuple<String,Long,Long,Long>( name, default_size, size, addr, init_align, init_align) );
		}//end if option
	}//end if section range
}//END populateSections_SCOUT
*/

/** Function: sortSymbols
 * @param None BUT assumes that you have run generateObjs_gcc.sh AND getSymbols.sh
 * @throws InterruptedException
 * @throws IOException
 * 
 * Using the nm output within txt files, reads in each line into 
 * arrays (contains size, type, and name of symbol) 
 * these are the first symbols to be put into the linker script for easy debugging
 * /
public static void sortSymbols()throws InterruptedException,IOException{
	//symbol type is at spot char[17]
	//symbol name is from spot char[19] to end
	List<String> nmByLine = new ArrayList<String>();
	String temp;
	
	//open the x86 text file
	FileReader fr1 = new FileReader(new File(Runtime.TARGET_DIR+"/nm_symbols_x86.txt"));
	BufferedReader br1 = new BufferedReader(fr1);
	//read into array
	while((temp = br1.readLine()) != null){
		nmByLine.add(temp);
	}
	//open the ARM text file
	fr1.close();
	br1.close();
	fr1 = new FileReader(new File(Runtime.TARGET_DIR+"/nm_symbols_aarch64.txt"));
	br1 = new BufferedReader(fr1);
	//read into array
	while((temp = br1.readLine()) != null){
		nmByLine.add(temp);
	}
	//clean Readers
	fr1.close();br1.close();
	int main_count=0;
	long init_align=1;
	int alreadyInserted=0;
	
	//categorize to either functions or data
	for(int i=0;i< nmByLine.size(); i++){
		temp = nmByLine.get(i).substring(19);
		for(Tuple<String,Long,Long,Long> a : LinkerIO.all){
			if(temp.equals(a.getName())){
				alreadyInserted=1;
				break;
			}
		}
		if(alreadyInserted == 1){
			alreadyInserted=0;
			continue;
		}
		
		// B or b (.bss symbol) also c or C (**COMMON** symbol) gets put here.
		if(Character.toLowerCase(nmByLine.get(i).charAt(17)) == 'c' || Character.toLowerCase(nmByLine.get(i).charAt(17)) == 'b'){
			//temp = nmByLine.get(i).substring(19);
			if (temp.indexOf(".") > 0){
				System.out.println("BSS OLD temp: "+temp);
				temp = temp.substring(0, temp.lastIndexOf("."));
				System.out.println("new: "+temp);
			}
			globalVars.A_bss.add(new Tuple<String,Long,Long,Long>(".bss."+temp,(long)0,(long)0,(long)0,init_align,init_align));
			System.out.println("bssSymbol: "+temp);
			++bssCount;
			LinkerIO.all.add(new Tuple<String,Long,Long,Long>(temp,(long)0,(long)0,(long)0,init_align,init_align));
		// D or d (.data symbol)
		} else if(Character.toLowerCase(nmByLine.get(i).charAt(17)) == 'd'){		
			//temp = nmByLine.get(i).substring(19);
			globalVars.A_data.add(new Tuple<String,Long,Long,Long>(".data."+temp,(long)0,(long)0,(long)0,init_align,init_align));
			System.out.println("dataSymbol: "+temp);
			++dataCount;
			LinkerIO.all.add(new Tuple<String,Long,Long,Long>(temp,(long)0,(long)0,(long)0,init_align,init_align));
		// R or r (.rodata symbol)
		} else if(Character.toLowerCase(nmByLine.get(i).charAt(17)) == 'r'){
			//temp = nmByLine.get(i).substring(19);
			if (temp.indexOf(".") > 0){
				System.out.println("BSS OLD temp: "+temp);
				temp = temp.substring(0, temp.lastIndexOf("."));
				System.out.println("new: "+temp);
			}
			globalVars.A_rodata.add(new Tuple<String,Long,Long,Long>(".rodata."+temp,(long)0,(long)0,(long)0,init_align,init_align));
			System.out.println("ROdataSymbol: "+temp);
			++rodataCount;
			LinkerIO.all.add(new Tuple<String,Long,Long,Long>(temp,(long)0,(long)0,(long)0,init_align,init_align));
		}
		// T or t (.text symbol)
		else if(Character.toLowerCase(nmByLine.get(i).charAt(17)) == 't'){
			//temp = nmByLine.get(i).substring(19);
			if(temp.startsWith("$",0))
				continue;
			globalVars.A_text.add(new Tuple<String,Long,Long,Long>(".text."+temp,(long)0,(long)0,(long)0,init_align,init_align));
			System.out.println("funcs: "+temp+ "   "+globalVars.A_text.size());
			++textCount;
			LinkerIO.all.add(new Tuple<String,Long,Long,Long>(temp,(long)0,(long)0,(long)0,init_align,init_align));
		}		
		//else its probably a U symbol to be added in by library
	}//end for nmByLine
}//end sortSymbols

static void alignAArch64() throws InterruptedException, IOException{
	System.out.println("start alignAArch64 symbol size");

	//text non-intersection removal
	int p;
	for(p=0;p<globalVars.A_text.size();p++){
		globalVars.A_text.get(p).setSize_aarch64(align8(globalVars.A_text.get(p).getSize_aarch64()));
	}
	//RODATA
	for(p=0;p<A_rodata.size();p++){
		A_rodata.get(p).setSize_aarch64(align8(A_rodata.get(p).getSize_aarch64()));
	}
	//DATA
	for(p=0;p<A_data.size();p++){
		A_data.get(p).setSize_aarch64(align8(A_data.get(p).getSize_aarch64()));
	}
	//BSS
	for(p=0;p<A_bss.size();p++){
		A_bss.get(p).setSize_aarch64(align8(A_bss.get(p).getSize_aarch64()));
	}
}

static Long align8(Long input){
	while(input % 8 != 0){
		input+=1;
	}
	return input;		
}

static void align_aarch64_MultipleSymbolDiffAddr(List<Tuple<String,Long,Long,Long>> list){
	for(int i=0; i < list.size(); i++){
		if(list.get(i).getCount() > 1){
			System.out.println("aarch Multiple Symbol Diff Addr: "+list.get(i).getName()+"    Aligned: 0x"+Long.toHexString(alignHex(list.get(i).getSize_aarch64())));
			list.get(i).setSize_aarch64(alignHex(list.get(i).getSize_aarch64()));
		}
	}
}
//aligns hex number already converted to long to the next 0x10 number
static Long alignHex(Long input){
	while(input % 16 != 0){
		input+=1;
	}		
	return input;
}

static Long align8_Val(Long input){
	long temp=0;
	while(input % 8 != 0){
		input+=1;
		temp+=1;
	}
	return temp;
}
//x86_64
		long difference;
		int sameAddrFlag=0;
		for(int a =0; a < globalVars.A_text.size(); a++){
			if(a!=0){
				sameAddrFlag = globalVars.A_text.get(a-1).getFlag();
			}
			//add in ALIGN rules
			if(a==0){
				//first alignment does not matter
				LinkerIO.x86_64_alignment.add( "\t"+" . = ALIGN(0x1000);");
				LinkerIO.x86_64_alignment.add( "\t*("+globalVars.A_text.get(a).getName()+");");
				LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x1000);");
				LinkerIO.aarch64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
				continue;
			}
			//else on later alignments, size of previous (a-1) function plays a part
			difference = globalVars.A_text.get(a-1).getSize_x86_64() - globalVars.A_text.get(a-1).getSize_aarch64();
			if( difference == 0 ){
				//no difference align the same
				if(globalVars.A_text.get(a).getAlignment_x86() > globalVars.A_text.get(a).getAlignment_aRM()){
					//use x86 alignment
	*/
//                                      LinkerIO.x86_64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_x86())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                                      LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_x86())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                              }else{
//                                      //use arm alignment
//                                      LinkerIO.x86_64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_aRM())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                                      LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_aRM())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
/*				}
				LinkerIO.x86_64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
				LinkerIO.aarch64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
			}else if( difference < 0 ){
				//aarch64 function version is larger than x86-64 function version
				//need to offset x86-64
				LinkerIO.x86_64_alignment.add("\t"+" . = . + "+ Math.abs(difference)+";" );
				if(globalVars.A_text.get(a).getAlignment_x86() > globalVars.A_text.get(a).getAlignment_aRM()){
					//use x86 alignment
					 * 
					 */
//                                      LinkerIO.x86_64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_x86())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                                      LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_x86())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                              }else{
					//use arm alignment
//                                      LinkerIO.x86_64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_aRM())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                                      LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_aRM())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
/*				}
					LinkerIO.x86_64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
					LinkerIO.aarch64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
			}else{
				//aarch64 function version is smaller than x86-64 function version
				//need to offset aarch64
				System.out.println("Need offset aarch64 for function: "+globalVars.A_text.get(a).getName()+" from fn:"+globalVars.A_text.get(a-1).getName()+"    x86sz:0x"+Long.toHexString(globalVars.A_text.get(a-1).getSize_x86_64())+"   aRMsz:0x"+Long.toHexString(globalVars.A_text.get(a-1).getSize_aarch64())+"    diff: "+difference);
				LinkerIO.aarch64_alignment.add("\t"+" . = . + "+ Math.abs(difference)+ ";" );
				if(globalVars.A_text.get(a).getAlignment_x86() > globalVars.A_text.get(a).getAlignment_aRM()){
					//use x86 alignment
					 * 
					 */
//                              LinkerIO.x86_64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_x86())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                                      LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_x86())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                              }else{
//                                      //use arm alignment
//                                      LinkerIO.x86_64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_aRM())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                                      LinkerIO.aarch64_alignment.add("\t"+" . = ALIGN(0x"+Long.toHexString(globalVars.A_text.get(a).getAlignment_aRM())+" ); /*Align for"+globalVars.A_text.get(a).getName()+"  */");
//                              }
//                                      LinkerIO.x86_64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
//                                      LinkerIO.aarch64_alignment.add("\t*("+globalVars.A_text.get(a).getName()+");");
//                      }
//              }
