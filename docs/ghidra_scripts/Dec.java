import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
public class Dec extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    PrintWriter pw = new PrintWriter(new FileWriter(a[0], true));
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
    for (int i=1;i<a.length;i++){
      Address ad = toAddr(a[i]);
      Function f = getFunctionContaining(ad);
      if (f==null){ disassemble(ad); createFunction(ad,null); f=getFunctionAt(ad);}
      if (f==null){ pw.println("// no func at "+a[i]); continue;}
      DecompileResults r = d.decompileFunction(f,120,monitor);
      pw.println("// ===== "+f.getName()+" @ "+f.getEntryPoint()+" (pedido "+a[i]+")");
      pw.println(r.decompileCompleted()? r.getDecompiledFunction().getC() : "// fallo: "+r.getErrorMessage());
    }
    pw.close();
  }
}
