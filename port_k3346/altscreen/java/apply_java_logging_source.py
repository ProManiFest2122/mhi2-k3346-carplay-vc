#!/usr/bin/env python3
"""Apply v8k VC diagnostic-only changes to Java SOURCE from the VC-enabled
edzenis/mib2high-carplay-rgi altscreen_dev branch. Never touches .class/.jar.
Fails if the source does not match the expected VC-enabled structure.
"""
from pathlib import Path
import sys

if len(sys.argv) != 2:
    raise SystemExit('usage: apply_java_logging_source.py PATH_TO_BAPBridge.java')
p = Path(sys.argv[1])
s = p.read_text(encoding='utf-8')
if 'V8KVcTrace.record(' in s:
    raise SystemExit('Already instrumented. No changes made.')
if not ('private static boolean VC_RGI_ENABLED = true;' in s and
        'private void configureVcPresentation() throws Exception {' in s):
    raise SystemExit('Not the VC-enabled Java source; no changes made.')

replacements = [
    ('    private void prepareVcPresentation() throws Exception {\n',
     '    private void prepareVcPresentation() throws Exception {\n'
     '        V8KVcTrace.record("PREPARE vcEnabled=" + VC_RGI_ENABLED);\n'),
    ('            captureVcPresentationState();\n            return;\n',
     '            captureVcPresentationState();\n'
     '            V8KVcTrace.record("CAPTURE_STATE_OK favored=" + vcSavedFavoredViewMode);\n'
     '            return;\n'),
    ('    private void configureVcPresentation() throws Exception {\n',
     '    private void configureVcPresentation() throws Exception {\n'
     '        V8KVcTrace.record("CONFIGURE vcEnabled=" + VC_RGI_ENABLED\n'
     '            + " csRef=" + (csRef != null));\n'),
    ('            csRef.updateRgActive(true);\n            vcFactoryRgStatePropagated = true;\n',
     '            csRef.updateRgActive(true);\n'
     '            V8KVcTrace.record("UPDATE_RG_ACTIVE_RETURNED_OK");\n'
     '            vcFactoryRgStatePropagated = true;\n'),
    ('                "CarPlay factory RGI"\n            );\n',
     '                "CarPlay factory RGI"\n            );\n'
     '            V8KVcTrace.record("SET_FAVORED_RGI_RETURNED_OK");\n'),
    ('    private void maintainVcPresentation() {\n',
     '    private void maintainVcPresentation() {\n'
     '        V8KVcTrace.record("MAINTAIN vcEnabled=" + VC_RGI_ENABLED\n'
     '            + " override=" + vcPresentationOverrideActive);\n'),
    ('                    "reassert after external VC mode change"\n                );\n',
     '                    "reassert after external VC mode change"\n                );\n'
     '                V8KVcTrace.record("VC_MODE_REASSERT_RETURNED_OK");\n'),
    ('    private void setVcFavoredViewMode(int mode, String reason)\n'
     '        throws Exception {\n',
     '    private void setVcFavoredViewMode(int mode, String reason)\n'
     '        throws Exception {\n'
     '        V8KVcTrace.record("VC_SET_FAVORED_BEGIN mode=" + mode + " reason=" + reason);\n'),
    ('        csRef.getClusterViewMode().setFavoredViewMode(mode);\n',
     '        csRef.getClusterViewMode().setFavoredViewMode(mode);\n'
     '        V8KVcTrace.record("VC_SET_FAVORED_RETURNED_OK mode=" + mode);\n'),
    ('            csRef.updateRgActive(true);\n',
     '            V8KVcTrace.record("UPDATE_RG_ACTIVE_BEGIN");\n'
     '            csRef.updateRgActive(true);\n'),
    ('    private void restoreVcPresentation() {\n',
     '    private void restoreVcPresentation() {\n'
     '        V8KVcTrace.record("RESTORE override=" + vcPresentationOverrideActive);\n'),
    ('                    vcSavedFavoredViewMode\n                );\n',
     '                    vcSavedFavoredViewMode\n                );\n'
     '                V8KVcTrace.record("RESTORE_FAVORED_RETURNED_OK mode="\n'
     '                    + vcSavedFavoredViewMode);\n'),
]
for old, new in replacements:
    if s.count(old) != 1:
        raise SystemExit(f'Unexpected source shape: target occurs {s.count(old)} times: {old!r}')
    s=s.replace(old,new,1)
p.write_text(s,encoding='utf-8')
print(f'VC_DIAGNOSTICS_SOURCE_INSTRUMENTED={len(replacements)}')
