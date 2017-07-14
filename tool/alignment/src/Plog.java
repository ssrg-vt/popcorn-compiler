// Pierre log --> plog

import java.io.File;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.util.concurrent.TimeUnit;

public class Plog {
	static String output_file = "/root/plog";

	static void plogInit() {
		File f = new File(output_file);
		f.delete();
		try {
			f.createNewFile();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	static void log(String str) {
		try {
			FileWriter fw = new FileWriter(output_file, true);
			BufferedWriter bw = new BufferedWriter(fw);
			bw.write(str);
			bw.flush();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	static void wait(int sec) {
		try {
			TimeUnit.SECONDS.sleep(sec);
		} catch (InterruptedException e) {
			System.out.println("sleep interrupted");
		}
	}

}
