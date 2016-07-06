import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;
import java.util.regex.Pattern;


public class LinkerIO {
	//read in x86_64 linker script to array
	static List<String> linkerScript_ByLine_x86_64 = new ArrayList<String>();
	static List<String> linkerScript_ByLine_aarch64 = new ArrayList<String>();
	static List<String> sectionSize_alignment = new ArrayList<String>();
	public static int x86_newlines;
	public static int aRM_newlines;
	
	static void resetLinkerNewLines(){
		x86_newlines =0;
		aRM_newlines =0;
	}
	
	static void incrementLinkerNewLines_x86(){
		++x86_newlines;
	}
	
	static void incrementLinkerNewLines_aRM(){
		++aRM_newlines;
	}
	
	static int getLinkerNewLines_x86(){
		return x86_newlines;
	}
	
	static int getLinkerNewLines_aRM(){
		return aRM_newlines;
	}	
	
	public static List<Tuple<String,Long,Long,Long>> rodataSymbols = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> dataSymbols = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> bssSymbols = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> funcs = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> all = new ArrayList<Tuple<String,Long,Long,Long>>();
	
	public static List<Tuple<String,Long,Long,Long>> rodataSymbols_lib = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> dataSymbols_lib = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> bssSymbols_lib = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> funcs_lib = new ArrayList<Tuple<String,Long,Long,Long>>();
	public static List<Tuple<String,Long,Long,Long>> all_lib = new ArrayList<Tuple<String,Long,Long,Long>>();
	
	public static List<String> intersection = new ArrayList<String>();
	
	static List<String> x86_64_text_alignment = new ArrayList<String>();
	static List<String> x86_64_rodata_alignment = new ArrayList<String>();
	static List<String> x86_64_data_alignment = new ArrayList<String>();
	static List<String> x86_64_bss_alignment = new ArrayList<String>();
	
	static List<String> aarch64_text_alignment = new ArrayList<String>();
	static List<String> aarch64_rodata_alignment = new ArrayList<String>();
	static List<String> aarch64_data_alignment = new ArrayList<String>();
	static List<String> aarch64_bss_alignment = new ArrayList<String>();
	
	
	
	static List<String> COPY_linkerScript_ByLine_x86_64;
	static List<String> COPY_linkerScript_ByLine_aarch64;
	
	static int x86_textLineOffset = 54;
	static int x86_rodataLineOffset = 73;
	static int x86_dataLineOffset = 80;
	static int x86_bssLineOffset = 91;
	
	static int aarch64_textLineOffset = 45;
	static int aarch64_rodataLineOffset = 63+1;
	static int aarch64_dataLineOffset = 70+2; 
	static int aarch64_bssLineOffset = 81+3; 

	static int x86_textendLine_Offset = 63;
	static int x86_rodataendLine_Offset = 77;
	static int x86_dataendLine_Offset = 156;
	static int x86_bssendLine_Offset = 173;
	
	static int aarch64_textendLine_Offset = 54;
	static int aarch64_rodataendLine_Offset = 68;
	static int aarch64_dataendLine_Offset = 153;
	static int aarch64_bssendLine_Offset = 173;
	
	static void resetLinkerScript(){
		linkerScript_ByLine_x86_64.clear();
		linkerScript_ByLine_aarch64.clear();
		//clear text
		x86_64_text_alignment.clear();
		aarch64_text_alignment.clear();
		//clear rodata
		x86_64_rodata_alignment.clear();
		aarch64_rodata_alignment.clear();
		//clear data
		x86_64_data_alignment.clear();
		aarch64_data_alignment.clear();
		//clear bss
		x86_64_bss_alignment.clear();
		aarch64_bss_alignment.clear();
	}
	
	static void readInLinkerScripts() throws IOException{
		//open the x86 linker script
		String temp;
		FileReader fr3 = new FileReader(new File(Runtime.TARGET_DIR+"/elf_x86_64.x"));
		BufferedReader br3= new BufferedReader(fr3);
		//read into array
		while((temp = br3.readLine()) != null){
			linkerScript_ByLine_x86_64.add(temp);
		}
	
		//open the aarch64 linker script
		FileReader fr4 = new FileReader(new File(Runtime.TARGET_DIR+"/aarch64linux.x"));
		BufferedReader br4= new BufferedReader(fr4);
		//read into array
		while((temp = br4.readLine()) != null){
			linkerScript_ByLine_aarch64.add(temp);
		}
	}
	
	static void writeOutLinkerScripts() throws IOException{
		//save the file and overwrite to become the new linker script.
		int x86_count=0, arm_count=0;
		PrintWriter wX = new PrintWriter(Runtime.TARGET_DIR+"/modified__elf_x86_64.x", "UTF-8");
		for(String s: linkerScript_ByLine_x86_64){
			wX.println(s);
			++x86_count;
		}
		wX.close();
		
		//do the same for arm
		PrintWriter wA = new PrintWriter(Runtime.TARGET_DIR+"/modified__aarch64.x", "UTF-8");
		for(String s: linkerScript_ByLine_aarch64){
			wA.println(s);
			++arm_count;
		}
		wA.close();
	}
	
	static void addAlignmentToLinkerScripts(List<String> x86_64_alignment, List<String> aarch64_alignment,int x86_fileLineOffset,int aarch64_fileLineOffset){
		String element="";
		int nlines=0;
		
		if(x86_64_alignment != null){
			for(String s: x86_64_alignment){
				element += s + "\n";
				++nlines;
				incrementLinkerNewLines_x86();
			}
			linkerScript_ByLine_x86_64.add(x86_fileLineOffset, element);
			System.out.println("added: "+nlines+" new lines of x86 linker script.");
		}
		if(aarch64_alignment != null){
			element="";
			for(String s: aarch64_alignment){
				element += s + "\n";
				++nlines;
				incrementLinkerNewLines_aRM();
			}
			linkerScript_ByLine_aarch64.add(aarch64_fileLineOffset, element);
			System.out.println("added: "+nlines+" new lines of ARM linker script.");
		}
	}
	
	static void addLineToLinkerScript(String alignment,int offset,int x86_OR_aarch){
		if(x86_OR_aarch == 0){
			//x86 needs alignment
			//System.out.println("TTTT: "+offset);
			linkerScript_ByLine_x86_64.add(offset, alignment);
		}else if(x86_OR_aarch ==1){
			//aarch64 needs alignment
			linkerScript_ByLine_aarch64.add(offset, alignment);
		}
		//else there is a problem!!!
	}
	static void alignSectionHeaders(){
		replaceLineInLinkerScript("\\p{Blank}*(\\.rodata)\\p{Blank}*:.*","  .rodata\t: ALIGN(0x1000)",1);
		replaceLineInLinkerScript("\\p{Blank}*(\\.rodata)\\p{Blank}*:.*","  .rodata\t: ALIGN(0x1000)",0);
		
		replaceLineInLinkerScript("\\p{Blank}*(\\.data)\\p{Blank}*:.*","  .data\t: ALIGN(0x1000)",1);
		replaceLineInLinkerScript("\\p{Blank}*(\\.data)\\p{Blank}*:.*","  .data\t: ALIGN(0x1000)",0);
		
		replaceLineInLinkerScript("\\p{Blank}*(\\.bss)\\p{Blank}*:.*","  .bss\t: ALIGN(0x1000)",1);
		replaceLineInLinkerScript("\\p{Blank}*(\\.bss)\\p{Blank}*:.*","  .bss\t: ALIGN(0x1000)",0);
	}
	
	static void replaceLineInLinkerScript(String findPatern,String replacePatern, int x86_OR_aarch){
		//0=x86 || 1=ARM
		
		// .rodata :   >>> regex is          "\\p{Blank}*(\\.rodata)\\p{Blank}*:.*"
		//System.out.println("inside replaceLine");
		ListIterator<String> li = linkerScript_ByLine_x86_64.listIterator();
		ListIterator<String> lo = linkerScript_ByLine_aarch64.listIterator();
		
		int line_num=-1;
		if(x86_OR_aarch == 0){
			//x86
			while(li.hasNext()) {
			    int i = li.nextIndex();
			    String next = li.next();
			    if(Pattern.matches(findPatern, next)){
			      line_num = i;
			      //System.out.println("Orig X86:"+linkerScript_ByLine_x86_64.get(line_num));
			    }
			}
			if(line_num!= -1){
				linkerScript_ByLine_x86_64.set(line_num,replacePatern);
				//System.out.println("New X86:"+linkerScript_ByLine_x86_64.get(line_num));
			}else{
				//System.out.println("pattern: "+findPatern +"  NOT FOUND!");
			}
		}else if(x86_OR_aarch ==1){
			//aarch64 needs alignment
			while(lo.hasNext()) {
			    int i = lo.nextIndex();
			    String next = lo.next();
			    if(Pattern.matches(findPatern, next)){
			      line_num = i;
			      //System.out.println("Orig ARM:"+linkerScript_ByLine_aarch64.get(line_num));
			    }
			}
			if(line_num!= -1){
				linkerScript_ByLine_aarch64.set(line_num,replacePatern);
				//System.out.println("New ARM:"+linkerScript_ByLine_aarch64.get(line_num));
			}else{
				//System.out.println("pattern: "+findPatern +"  NOT FOUND!");
			}
		}
		//else there is a problem!!!
	}

}//end LinkerIO
