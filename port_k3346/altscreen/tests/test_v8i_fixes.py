from pathlib import Path
import re, hashlib
root=Path(__file__).resolve().parents[1]
a=(root/'reference/bundles_dsitracer_enabled.v8g2.original.properties').read_bytes()
b=(root/'config/bundles_dsitracer_enabled.v8i.merged.properties').read_bytes()
s=(root/'reference/bundles.v8g2.original.properties').read_bytes()
assert len(b)>len(a)
assert a.count(b'MIB2HighCarPlayRGI')==0
assert b.count(b'MIB2HighCarPlayRGI')==2
assert b.count(b'TraceConfigurator')==a.count(b'TraceConfigurator')
assert b.replace(b',MIB2HighCarPlayRGI',b'').replace(b'MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator\n',b'')==a
assert b'MIB2HighCarPlayRGI=org.mib2high.carplay.rgi.Activator' in s
assert b'DSITracer,FwUtilServices,Profiling,Tracing,ATIPImpl,MIB2HighCarPlayRGI' in b
native=(root/'native/altscreen_session.c').read_text()
assert native.count('dict_set_string(params, "uuid", ALTSCREEN_UUID)')>=1
assert 'V8I_ZOOM' in native
assert 'ack=UNKNOWN' in native
alt=(root/'native/altscreen_airplay.c').read_text()
assert 'ALTSCREEN_DECLARED_CANVAS_H' not in alt
assert 'set_i64(alt, "heightPixels", profile.h)' in alt
assert 'set_i64(area, "heightPixels", p->h)' in alt
assert 'physical 200 75' in (root/'config/mib2high-lvds-viewareas.conf').read_text()
print('V8I_SOURCE_FIX_TEST=PASS')

trace=(root/'config/traceConfig.v8g2.enabled.properties').read_text()
assert '\norg.dsi.ifc.map.DSIMapViewerControl=full' in trace
assert 'V8I_GAUGE_CANDIDATE validated=0' in native

assert (root/'config/mib2high-lvds-profile.conf').read_text().startswith('1440 542 30 ')
assert 'advertised=%dx%d' in alt
