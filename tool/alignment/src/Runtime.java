import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class Runtime {
	
	static String TARGET_DIR;
	static String COMPILE_OPT; // Zero (0) is HOMOGENEOUS, One (1) is HETEROGENEOUS
	static String SOURCE_DIR = System.getProperty("user.dir"); //+"/../";
	//static String INCLUDE_DIR="../../libc/install/x86_64-linux-gnu/lib";
	static List<String> utilityARG_list = new ArrayList<String>();
	
	static void checkProcess(Process p, String runtime) throws InterruptedException{
		if(p.waitFor() == 0){
			if(globalVars.DEBUG)
				System.out.println("Success:  "+runtime+"  !");
		}else{
			System.out.println("\n_________ FAILURE: "+runtime+"  !");
			System.exit(0);
		}
	}
	
	static void runScript(String scriptName, List<String> Args) throws IOException, InterruptedException{
		List<String> commands = new ArrayList<String>();
		commands.add("/bin/bash");
		commands.add(scriptName);
		if(Args != null){
			commands.addAll(Args);
		}			
		if(globalVars.DEBUG) System.out.println("DEBUG: "+ commands);
		
		//running the command
		ProcessBuilder pb = new ProcessBuilder(commands);
		pb.directory(new File(TARGET_DIR));
		pb.redirectErrorStream(true);
		Process p = pb.start();
	
		//check result
		checkProcess(p, scriptName);
	}
	
	/**
	 * MAIN
	 * @param args
	 * @throws IOException
	 * @throws InterruptedException
	 */
	public static void main(String[] args) throws IOException, InterruptedException {
		LinkerIO.resetLinkerNewLines();
		
		if(globalVars.DEBUG) System.out.println("MODULAR GENERIC ALIGNMENT");
		COMPILE_OPT = args[0];
		TARGET_DIR = args[1]+"/align";
		boolean COMPILE_VAN = false;
		for(int len = 0 ; len < args.length; ++len){
			if(args[len].compareTo("-skip-vanilla") == 0){
				COMPILE_VAN = false;
				break;
			}
		}
		if(globalVars.DEBUG){
			System.out.println("Source: "+SOURCE_DIR);
			System.out.println("Target: "+TARGET_DIR);
			if(COMPILE_OPT.equals("0")){
				System.out.println("Compilation: "+COMPILE_OPT+" HOMO-GENEOUS");
			}else if(COMPILE_OPT.equals("1")){
				System.out.println("Compilation: "+COMPILE_OPT+" HETERO-GENEOUS");
			}
		}
		
		if(COMPILE_VAN){
			/************************ Begin VANILLA RUN ********************************************/
			if(globalVars.DEBUG) System.out.println("----------------------- Clean Directory! --------------------------------");
			//clean everything
			cleanTargetDIR("4");
		
			if(globalVars.DEBUG) System.out.println("----------------------- Copy scripts to target Directory --------------------");
			copyToolToTargetDIR();
			if(globalVars.DEBUG) System.out.println("----------------------- Generate Obj files! ---------------------------------");
			utilityARG_list.add(COMPILE_OPT);
			runScript("generateObjs_clang.sh",utilityARG_list);
			utilityARG_list.clear();
		
			if(globalVars.DEBUG) System.out.println("----------------------- ^ Link 1: No Alignment, No custom LinkerScript ---");
			List<String> vanillaARGSx86 = new ArrayList<String>();
			vanillaARGSx86.add("0");
			vanillaARGSx86.add("obj_x86.txt");
			runScript("mlink_x86Objs.sh",vanillaARGSx86);
		
			List<String> vanillaARGSarm = new ArrayList<String>();
			vanillaARGSarm.add("0");
			vanillaARGSarm.add("obj_arm.txt");
			runScript("mlink_armObjs.sh",vanillaARGSarm);
			if(globalVars.DEBUG){
				System.out.println("----------------------- Vanilla Phase Complete! -----------------------------");
				System.out.println("----------------------- ^^^^^^^^^^^^^^^^^^^^^^^------------------------------");
				System.out.println("\n");
			}
		}//END IF COMPILE_VANILLA

		
		if(globalVars.DEBUG) System.out.println("-1--------------------- Gather & Sort Symbols Begin! ----------------------------------");
		//find unique for each architecture
		gatherANDsort();
		if(globalVars.DEBUG) System.out.println("-1--------------------- Gather & Sort Symbols Complete --------------------------------");
		LinkerIO.readInLinkerScripts();
   
PrintWriter writer = new PrintWriter(TARGET_DIR+"/v1113sizetext.txt", "UTF-8");
for(int w =0; w < globalVars.A_rodata.size(); w++){
writer.println(globalVars.A_rodata.get(w).getName()+"\t\taddr:0x"+Long.toHexString(globalVars.A_text.get(w).getAddr())+"\t\tx86size:0x"+Long.toHexString(globalVars.A_text.get(w).getSize_x86_64())+"\t\t\taRMsize:0x"+Long.toHexString(globalVars.A_text.get(w).getSize_aarch64()));
}
writer.close();

		if(globalVars.DEBUG) System.out.println("----------------------- ReadInLinkerScripts Complete! ---------------------------------");
		
		if(globalVars.DEBUG) System.out.println("\n--------------------- Rerunning with added Nuances ----------------------------------");
		LinkerIO.alignSectionHeaders();
		
		//add initial movement of text symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_text, LinkerIO.x86_64_text_alignment,LinkerIO.aarch64_text_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_text_alignment, LinkerIO.aarch64_text_alignment,LinkerIO.x86_textLineOffset,LinkerIO.aarch64_textLineOffset);
		//add initial movement of rodata symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_rodata, LinkerIO.x86_64_rodata_alignment,LinkerIO.aarch64_rodata_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_rodata_alignment, LinkerIO.aarch64_rodata_alignment,LinkerIO.x86_rodataLineOffset,LinkerIO.aarch64_rodataLineOffset);
		//add initial movement of data symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_data, LinkerIO.x86_64_data_alignment, LinkerIO.aarch64_data_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_data_alignment, LinkerIO.aarch64_data_alignment,LinkerIO.x86_dataLineOffset,LinkerIO.aarch64_dataLineOffset);
		//add initial movement of bss symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_bss, LinkerIO.x86_64_bss_alignment, LinkerIO.aarch64_bss_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_bss_alignment, LinkerIO.aarch64_bss_alignment,LinkerIO.x86_bssLineOffset,LinkerIO.aarch64_bssLineOffset);
		
		/************** TLS */
		//add initial movement of TLS data symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_data_TLS, LinkerIO.x86_64_tls_data_alignment, LinkerIO.aarch64_tls_data_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_tls_data_alignment, LinkerIO.aarch64_tls_data_alignment,LinkerIO.x86_tlsdataLineOffset,LinkerIO.aarch64_tlsdataLineOffset);	
		//add initial movement of TLSbss symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_bss_TLS, LinkerIO.x86_64_tls_bss_alignment, LinkerIO.aarch64_tls_bss_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_tls_bss_alignment, LinkerIO.aarch64_tls_bss_alignment,LinkerIO.x86_tlsbssLineOffset,LinkerIO.aarch64_tlsbssLineOffset);
		
		if(globalVars.DEBUG) System.out.println("----------------------- Initial Alignments Added!----------------------------------------");
		
		List<String> x86withChanges = new ArrayList<String>();
		x86withChanges.add("1");
		x86withChanges.add("obj_x86.txt");
		List<String> armwithChanges = new ArrayList<String>();
		armwithChanges.add("1");
		armwithChanges.add("obj_arm.txt");
		LinkerIO.writeOutLinkerScripts();
		
		if(globalVars.DEBUG) System.out.println("\n--------------------- Rerunning with modified linker aligned script");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		
		if(globalVars.DEBUG) System.out.println("----------------------- End of Section **** -------------------------------------------");
		LinkerIO.resetLinkerNewLines();
		utilityARG_list.add("4");
		runScript("readMyElfToFile.sh",utilityARG_list);
		utilityARG_list.clear();
		
		AlignmentLogic.recordRanges(0);
		AlignmentLogic.recordRanges(1);

		long sneaky = AlignmentLogic.AntonioOffsetAdditional("text");
		AlignmentLogic.add_sectionAligment("text",sneaky);
		LinkerIO.writeOutLinkerScripts();
		if(globalVars.DEBUG) System.out.println("\n--------------------- Rerunning with added **** ----------------------------------");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		if(globalVars.DEBUG) System.out.println("-Text-------------------- Add **** Complete! -------------------------------------------");
		
		/*
		//next align rodata
		utilityARG_list.add("4");
		runScript("nmScript.sh",utilityARG_list);
		utilityARG_list.clear();
		AlignmentLogic.compareAndSet_Alignment(LinkerIO.rodataSymbols, LinkerIO.x86_rodataLineOffset, LinkerIO.aarch64_rodataLineOffset);
		LinkerIO.writeOutLinkerScripts();
		
		System.out.println("\nRODATA:  modifications done, rerunning with modified linker aligned script");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		*/
		if(globalVars.DEBUG) System.out.println("----------------------- End of Section **** -------------------------------------------");
		AlignmentLogic.resetRangesInfo();
		utilityARG_list.add("4");
		runScript("readMyElfToFile.sh",utilityARG_list);
		utilityARG_list.clear();
				
		AlignmentLogic.recordRanges(0);
		AlignmentLogic.recordRanges(1);
		
		sneaky = AlignmentLogic.AntonioOffsetAdditional("rodata");
		AlignmentLogic.add_sectionAligment("rodata",sneaky);
		LinkerIO.writeOutLinkerScripts();
		if(globalVars.DEBUG) System.out.println("\n--------------------- Rerunning with added **** ----------------------------------");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		if(globalVars.DEBUG) System.out.println("-Rodata-------------------- Add **** Complete! -------------------------------------------");
		
		/*
		//next align data
		utilityARG_list.add("4");
		runScript("nmScript.sh",utilityARG_list);
		utilityARG_list.clear();
		AlignmentLogic.compareAndSet_Alignment(LinkerIO.dataSymbols, LinkerIO.x86_dataLineOffset, LinkerIO.aarch64_dataLineOffset);
		LinkerIO.writeOutLinkerScripts();
		System.out.println("\nDATA:  modifications done, rerunning with modified linker aligned script");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		*/
		if(globalVars.DEBUG) System.out.println("----------------------- End of Section **** -------------------------------------------");
		AlignmentLogic.resetRangesInfo();
		utilityARG_list.add("4");
		runScript("readMyElfToFile.sh",utilityARG_list);
		utilityARG_list.clear();

		AlignmentLogic.recordRanges(0);
		AlignmentLogic.recordRanges(1);

		sneaky =AlignmentLogic.AntonioOffsetAdditional("data");
		AlignmentLogic.add_sectionAligment("data", sneaky);
		LinkerIO.writeOutLinkerScripts();
		if(globalVars.DEBUG) System.out.println("\n--------------------- Rerunning with added **** ----------------------------------");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		if(globalVars.DEBUG) System.out.println("-Data-------------------- Add **** Complete! -------------------------------------------");
		
		/*
		//next align bss
		utilityARG_list.add("4");
		runScript("nmScript.sh",utilityARG_list);
		utilityARG_list.clear();		
		AlignmentLogic.compareAndSet_Alignment(LinkerIO.bssSymbols, LinkerIO.x86_bssLineOffset, LinkerIO.aarch64_bssLineOffset);
		LinkerIO.writeOutLinkerScripts();
		System.out.println("BSS:  modifications done, rerunning with modified linker aligned script");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		*/
		
		/*
		System.out.println("----------------------- End of Section **** -------------------------------------------");
		AlignmentLogic.resetRangesInfo();
		utilityARG_list.add("4");
		//option 1 to read musl binarys
		runScript("readMyElfToFile.sh",utilityARG_list);
		utilityARG_list.clear();
		
		AlignmentLogic.recordRanges(0);
		AlignmentLogic.recordRanges(1);

		//AlignmentLogic.add_sectionAligment("bss");
		AlignmentLogic.AntonioOffsetAdditional("bss");
		LinkerIO.writeOutLinkerScripts();
		System.out.println("\n--------------------- Rerunning with added **** ----------------------------------");
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		System.out.println("-BSS-------------------- Add **** Complete! -------------------------------------------");
		LinkerIO.writeOutLinkerScripts();
		//END END OF SECTION ****
		*/
		if(!globalVars.DEBUG){
			cleanTargetDIR("3");
		}
		System.out.println("<success> END!");	
	}//end MAIN!!
	
	/** TODO: gatherANDsort **/
	static void gatherANDsort() throws InterruptedException, IOException{
		//parse elf - get elf info
		utilityARG_list.add("3");
		runScript("readMyElfToFile.sh",utilityARG_list);
		utilityARG_list.clear();
		
		if(globalVars.DEBUG) System.out.println("-1---------------------- recordRanges -------------------------------------------");
		//save section ranges x86
		AlignmentLogic.recordRanges(0);
		//save section ranges ARM
		AlignmentLogic.recordRanges(1);
		if(globalVars.DEBUG) System.out.println("-1--------------------- recordRanges Complete! ----------------------------------");

		if(globalVars.DEBUG){
			//PRINT NM Files
			utilityARG_list.add("0");
			runScript("nmScript.sh",utilityARG_list);
			utilityARG_list.clear();
		}
		
		/** SCOUT for symbols and layout**/
		/***************************************************************************************************************************************/
		/***************************************************************************************************************************************/
		/***************************************************************************************************************************************/
		if(globalVars.DEBUG) System.out.println("-1---------------------- grabSymbolsInRange --------------------------------------");
		AlignmentLogic.grabSymbolsInRange(0,"exe");
		globalVars.resetMultipleAddress();
		AlignmentLogic.grabSymbolsInRange(1,"exe");
		globalVars.resetMultipleAddress();
		if(globalVars.DEBUG) System.out.println("-1---------------------- grabSymbolsInRange Complete!-----------------------------");
		
		if(globalVars.DEBUG) System.out.println("-1---------------------- Clean Intersection --------------------------------------");
		AlignmentLogic.cleanIntersection();
		if(globalVars.DEBUG) System.out.println("-1---------------------- Clean Intersection Complete!-----------------------------");
		
		if(globalVars.DEBUG) System.out.println("-1--------------------- ^^ Compile 2: No Alignment, Custom Linker Script Order----");
		//do scout compilation
		if(globalVars.DEBUG) System.out.println("-1--------------------- ^^ ReadInLinkerScripts -----------------------------------");
		LinkerIO.readInLinkerScripts();
		if(globalVars.DEBUG) System.out.println("-1--------------------- ^^ ReadInLinkerScripts Complete!--------------------------");
		
		//add initial movement of text
		AlignmentLogic.set_symbol_Alignment(globalVars.A_text, LinkerIO.x86_64_text_alignment, LinkerIO.aarch64_text_alignment,false);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_text_alignment, LinkerIO.aarch64_text_alignment, LinkerIO.x86_textLineOffset, LinkerIO.aarch64_textLineOffset);
		LinkerIO.alignSectionHeaders();
		//add initial movement of rodata
		AlignmentLogic.set_symbol_Alignment(globalVars.A_rodata, LinkerIO.x86_64_rodata_alignment, LinkerIO.aarch64_rodata_alignment,false);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_rodata_alignment, LinkerIO.aarch64_rodata_alignment, LinkerIO.x86_rodataLineOffset, LinkerIO.aarch64_rodataLineOffset);
		//add initial movement of data symbols
		AlignmentLogic.set_symbol_Alignment(globalVars.A_data, LinkerIO.x86_64_data_alignment, LinkerIO.aarch64_data_alignment,false);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_data_alignment, LinkerIO.aarch64_data_alignment,LinkerIO.x86_dataLineOffset,LinkerIO.aarch64_dataLineOffset);
		//add initial movement of bss symbols
		AlignmentLogic.set_symbol_Alignment(globalVars.A_bss, LinkerIO.x86_64_bss_alignment, LinkerIO.aarch64_bss_alignment,false);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_bss_alignment, LinkerIO.aarch64_bss_alignment,LinkerIO.x86_bssLineOffset,LinkerIO.aarch64_bssLineOffset);
		
		/************** TLS */
		//add initial movement of TLS data symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_data_TLS, LinkerIO.x86_64_tls_data_alignment, LinkerIO.aarch64_tls_data_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_tls_data_alignment, LinkerIO.aarch64_tls_data_alignment,LinkerIO.x86_tlsdataLineOffset,LinkerIO.aarch64_tlsdataLineOffset);		
		//add initial movement of TLSbss symbols
		AlignmentLogic.set_symbol_AlignmentN(globalVars.A_bss_TLS, LinkerIO.x86_64_tls_bss_alignment, LinkerIO.aarch64_tls_bss_alignment,0,0,0,true);
		LinkerIO.addAlignmentToLinkerScripts(LinkerIO.x86_64_tls_bss_alignment, LinkerIO.aarch64_tls_bss_alignment,LinkerIO.x86_tlsbssLineOffset,LinkerIO.aarch64_tlsbssLineOffset);
		
		
		List<String> x86withChanges = new ArrayList<String>();
		x86withChanges.add("1");
		x86withChanges.add("obj_x86.txt");
		List<String> armwithChanges = new ArrayList<String>();
		armwithChanges.add("1");
		armwithChanges.add("obj_arm.txt");
		LinkerIO.writeOutLinkerScripts();
		runScript("mlink_x86Objs.sh",x86withChanges);
		runScript("mlink_armObjs.sh",armwithChanges);
		/** END SCOUT for symbols and layout **/
		if(globalVars.DEBUG) System.out.println("-1--------------------- ^^ Compile 2: COMPLETE ---------------------------------\n");
		
		if(globalVars.DEBUG) System.out.println("-1--------------------- ^^ Reseting Select Storages");
		//reset ALMOST everything
		globalVars.resetSectionsInfo();
		LinkerIO.resetLinkerScript();
		AlignmentLogic.resetRangesInfo();
		if(globalVars.DEBUG) System.out.println("-1--------------------- ^^ ########### Reset Complete ########### -----------");
		/** TODO: Final Compile Start **/
		/***************************************************************************************************************************************/
		/***************************************************************************************************************************************/
		/***************************************************************************************************************************************/
		/***************************************************************************************************************************************/
		//COMPILATION #3 Map now has symbols laid out in same order BUT NOT aligned.
		utilityARG_list.add("4");
		//option 1 to read musl binarys
		runScript("readMyElfToFile.sh",utilityARG_list);
		utilityARG_list.clear();
		
		AlignmentLogic.recordRanges(0);
		AlignmentLogic.recordRanges(1);
		
		utilityARG_list.add("4");
		runScript("nmScript.sh",utilityARG_list);
		utilityARG_list.clear();
		
		//Necessary for accurate "** fill" information 
		AlignmentLogic.grabSymbolsInRange(0,"gold");
		globalVars.resetMultipleAddress();
		AlignmentLogic.grabSymbolsInRange(1,"gold");
		globalVars.resetMultipleAddress();
		
		/*//SANITY write to File
		PrintWriter writer99 = new PrintWriter(TARGET_DIR+"/size.txt", "UTF-8");
		for(int w =0; w < globalVars.A_text.size(); w++){
			writer99.println(globalVars.A_text.get(w).getName()+"\t\tx86addr:0x"+globalVars.A_text.get(w).getAddr()+"\t\tx86size:0x"+Long.toHexString(globalVars.A_text.get(w).getSize_x86_64())+"\t\taRMsize:0x"+Long.toHexString(globalVars.A_text.get(w).getSize_aarch64()));
		}
		writer99.close();*/
		if(globalVars.DEBUG) System.out.println("-1---------------------- Clean Intersection --------------------------------------");
		AlignmentLogic.cleanIntersection();

	}//END gatherANDsort
	
	/**
	 * @param option
	 * 	Option 4: EVERYTHING"
		Option 3: all scripts, text, linker files (****.x), intermediate Bins + Objs"
		Option 2: all text, linker files (***.x), intermediate Bins + Objs"
		Option 1: all intermediate Bins + Objs"
		Option *: Nothing"
	 * @throws InterruptedException
	 * @throws IOException
	 */
	static void cleanTargetDIR(String option) throws InterruptedException,IOException{
		utilityARG_list.add("cp");
		utilityARG_list.add(SOURCE_DIR+"/../scriptsMaster/clean.sh");
		utilityARG_list.add(TARGET_DIR+"/");
		
		ProcessBuilder pb = new ProcessBuilder(utilityARG_list);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		Process p = pb.start();
		
		//check result
		checkProcess(p, "cleanTargetDIR");
		
		utilityARG_list.clear();
		utilityARG_list.add(option);
	
		runScript("clean.sh",utilityARG_list);
		utilityARG_list.clear();
	}
	
	static void copyToolToTargetDIR()throws InterruptedException,IOException{
		//System.out.println("start copyToolToTargetDIR");
		List<String> commands = new ArrayList<String>();
		ProcessBuilder pb;
		Process p;
		
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/clean.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/.gitignore");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/mlink_armObjs.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/mlink_x86Objs.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG)  System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/generateObjs_clang.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/getSymbols.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/nmScript.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);

		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();

		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/readMyElfToFile.sh");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);

		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();

		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/elf_x86_64.x");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		commands.add("cp");
		commands.add(SOURCE_DIR+"/../scriptsMaster/aarch64linux.x");
		commands.add(TARGET_DIR+"/");
		if(globalVars.DEBUG) System.out.println("cpyTool: "+commands);
		
		//running the command
		pb = new ProcessBuilder(commands);
		pb.directory(new File(SOURCE_DIR));
		pb.redirectErrorStream(true);
		p = pb.start();
		
		//check result
		checkProcess(p, "copyTool");
		commands.clear();
		////////////////////////////////////////////////////////////////////////////////////////////////////////
		if(globalVars.DEBUG) System.out.println("Copy To TARGET DIR complete!");
	}	
}
