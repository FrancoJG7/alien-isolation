import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import java.io.*;
// args: out pattern... ; lista simbolos y strings que contienen el patron, con xrefs
public class Sym extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    PrintWriter pw = new PrintWriter(new FileWriter(a[0], true));
    for (int i=1;i<a.length;i++){
      String p=a[i]; boolean exact=p.startsWith("=");
      if(exact)p=p.substring(1);
      pw.println("#### "+a[i]);
      SymbolIterator it=currentProgram.getSymbolTable().getSymbolIterator(exact? p : "*"+p+"*", true);
      int n=0;
      while(it.hasNext()&&n<60){Symbol s=it.next(); String nm;
        try{ nm=s.getName(true);}catch(Throwable t){nm=s.getName();}
        if(nm.contains("RTTI_"))continue; n++;
        String r; try{r=refs(s.getAddress());}catch(Throwable t){r=" (refs err)";}
        pw.println("SYM "+nm+" @ "+s.getAddress()+r);}
      DataIterator di=currentProgram.getListing().getDefinedData(true); n=0;
      while(di.hasNext()&&n<40){Data d=di.next(); if(!(d.getValue() instanceof String))continue;
        String v=(String)d.getValue(); if(exact? v.equals(p): v.contains(p)){n++;
          String r; try{r=refs(d.getAddress());}catch(Throwable t){r=" (refs err)";}
          pw.println("STR \""+v+"\" @ "+d.getAddress()+r);}}
    }
    pw.close();
  }
  String refs(Address ad){StringBuilder sb=new StringBuilder(); int k=0;
    for(Reference r: getReferencesTo(ad)){ if(k++>12){sb.append(" ...");break;}
      Function f=getFunctionContaining(r.getFromAddress());
      sb.append(" <- "+r.getFromAddress()+(f!=null?"("+f.getName()+"@"+f.getEntryPoint()+")":""));}
    return sb.toString();}
}
