import os, struct, base64, json

dll_path = r'C:\Program Files\Common Files\Avid\Audio\Plug-Ins\Crane Song\PhoenixII.aaxplugin\Contents\Resources\PhoenixToo_Stereo.dll'
out_dir = r'C:\Users\anton\Desktop\PhoenixII-Clone'
assets_dir = os.path.join(out_dir, 'extracted_assets')

with open(dll_path, 'rb') as f:
    dll = f.read()

# Extract 24576 floats of PHXTBLDATA from offset 0x40
tbl_floats = struct.unpack_from('<24576f', dll, 0x40)
print('PHXTBLDATA count:', len(tbl_floats), 'min:', min(tbl_floats), 'max:', max(tbl_floats))
print('Table 0 [0, 4096, 8191]:', tbl_floats[0], tbl_floats[4096], tbl_floats[8191])
print('Table 1 [8192, 12288, 16383]:', tbl_floats[8192], tbl_floats[12288], tbl_floats[16383])
print('Table 2 [16384, 20480, 24575]:', tbl_floats[16384], tbl_floats[20480], tbl_floats[24575])

# Write C++ header with exact PHXTBLDATA and Phoenix_Color_Types
cpp_hdr = os.path.join(out_dir, 'PhoenixDSPData.h')
with open(cpp_hdr, 'w', encoding='utf-8') as f:
    f.write('// Auto-extracted 1:1 from Crane Song PhoenixII.aaxplugin / PhoenixToo_Stereo.dll\n')
    f.write('// Plugin Developer / Creator: papielguapo\n')
    f.write('#pragma once\n#include <stdint.h>\n\n')
    f.write('struct PhoenixColorType {\n')
    f.write('    float brightCoeffs[3][2]; // [0]=Opal, [1]=Gold, [2]=Sapphire -> {coeffPre, coeffPost}\n')
    f.write('    float driveScale;\n')
    f.write('    float preEmphasisGain;\n')
    f.write('    float satMixGain;\n')
    f.write('    float dryCompGain;\n')
    f.write('    int32_t flagA;\n')
    f.write('    int32_t tableOffset;\n')
    f.write('};\n\n')
    f.write('static const float kSrFactorsTable[3] = { 1.0f, 0.5f, 0.25f };\n\n')
    f.write('static const PhoenixColorType kPhoenixColorTypes[5] = {\n')
    phx_color_ofs = 0x40 + (0x20020 - 0x8020)
    names = ['Luminescent', 'Iridescent', 'Radiant', 'Luster', 'Dark Essence']
    def fmt_f(v):
        s = f'{v:.9g}'
        if '.' not in s and 'e' not in s and 'E' not in s:
            s += '.0'
        return s + 'f'

    for i in range(5):
        chunk = dll[phx_color_ofs + i*48 : phx_color_ofs + (i+1)*48]
        fl = struct.unpack('<10fii', chunk)
        f.write(f'    // {i+1}: {names[i]}\n')
        f.write(f'    {{ {{{{{fmt_f(fl[0])}, {fmt_f(fl[1])}}}, {{{fmt_f(fl[2])}, {fmt_f(fl[3])}}}, {{{fmt_f(fl[4])}, {fmt_f(fl[5])}}}}}, '
                f'{fmt_f(fl[6])}, {fmt_f(fl[7])}, {fmt_f(fl[8])}, {fmt_f(fl[9])}, {fl[10]}, {fl[11]} }},\n')
    f.write('};\n\n')
    f.write('static const float kPhxTblData[24576] = {\n')
    for i in range(0, 24576, 8):
        row = ', '.join(fmt_f(v) for v in tbl_floats[i:i+8])
        f.write(f'    {row},\n')
    f.write('};\n')
print('Wrote', cpp_hdr, os.path.getsize(cpp_hdr), 'bytes')

# Write JS data file with PHXTBLDATA and PNG assets in base64
png_files = {
    'faceplate': 'res_PNG_31011_515x161.png',
    'knob_cont': 'res_PNG_31010_60x8100.png',
    'knob_bright': 'res_PNG_31013_60x180.png',
    'knob_type': 'res_PNG_31015_60x300.png',
}
assets_b64 = {}
for k, fname in png_files.items():
    with open(os.path.join(assets_dir, fname), 'rb') as pf:
        assets_b64[k] = 'data:image/png;base64,' + base64.b64encode(pf.read()).decode('ascii')

raw_tbl_bytes = dll[0x40 : 0x40 + 24576*4]
tbl_b64 = base64.b64encode(raw_tbl_bytes).decode('ascii')

js_data_path = os.path.join(out_dir, 'phoenix_dsp_data.js')
with open(js_data_path, 'w', encoding='utf-8') as f:
    f.write('// Auto-extracted 1:1 DSP tables & original PNG resources from Crane Song PhoenixII.aaxplugin\n')
    f.write('// Developer / Creator: papielguapo\n')
    f.write(f'window.PHOENIX_RAW_TABLE_B64 = "{tbl_b64}";\n')
    f.write('window.PHOENIX_ORIGINAL_ASSETS = ' + json.dumps(assets_b64) + ';\n')
print('Wrote', js_data_path, os.path.getsize(js_data_path), 'bytes')
