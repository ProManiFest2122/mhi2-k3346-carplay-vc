BEGIN{aut=0;jar=0}
/^Bundle.Startup.Autostart[[:space:]]*=/ {aut++; if(index($0,"MIB2HighCarPlayRGI")==0) $0=$0 ",MIB2HighCarPlayRGI"}
/^MIB2HighCarPlayRGI=/ {jar++}
{print}
END{if(aut!=1)exit 7; if(jar==0)print "MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator"; if(jar>1)exit 8}
