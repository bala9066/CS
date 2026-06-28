"""
Generate test CSV + ODT files for validating the Checksum VDD Auditor.
Creates files on the user's Desktop with project DP-TST-4455.
"""
import csv
import os
import random
import zipfile
import shutil
import tempfile

PROJECT = 'DP-TST-4455'
DOC_ID = '10499'
DESKTOP = os.path.join(os.path.expanduser('~'), 'OneDrive', 'Desktop')

# CI entries derived FROM the ODT's actual CI references (ensures matching)
ci_entries = [
    (1,  'Power Sequencer Prog File',  f'{PROJECT}-000-PWRSEQ-1V00',               '1V00', '', 'Non-Deliverable', ''),
    (2,  'ClockBuilder Prog File',     f'{PROJECT}-000-CLKPRO-1V00',               '1V00', '', 'Non-Deliverable', ''),
    (3,  'Library Output (ALGO)',      f'{PROJECT}-V3-LIBO-ALGO-ARM64-NOS-1V00',   '1V00', '', 'Deliverable', ''),
    (4,  'Firmware Executable (ALGO)', f'{PROJECT}-V3-FWO-ALGO-ARM64-NOS-1V00',    '1V00', '', 'Non-Deliverable', ''),
    (5,  'Flash Executable (ALGO)',    f'{PROJECT}-V3-FO-ALGO-ARM64-NOS-1V00',     '1V00', '', 'Non-Deliverable', ''),
    (6,  'FPGA Bitstream (ALGO)',      f'{PROJECT}-V2-01-U105-032D07D0',           'V2',   '', 'Non-Deliverable', ''),
    (7,  'App Executable (AUDIO)',     f'{PROJECT}-V3-AO-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Non-Deliverable', ''),
    (8,  'Flash Executable (AUDIO)',   f'{PROJECT}-V3-FO-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Non-Deliverable', ''),
    (9,  'Driver Executable (AUDIO)',  f'{PROJECT}-V3-DO-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Deliverable', ''),
    (10, 'OS Output (AUDIO)',          f'{PROJECT}-V3-OSO-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Non-Deliverable', ''),
    (11, 'SDK Tools',                  f'{PROJECT}-V3-TOOLS-SDK-ARM64-PLNX2019V2-1V00', '1V00', '', 'Non-Deliverable', 'SDK Toolchain'),
    (12, 'FPGA Bitstream (AUDIO)',     f'{PROJECT}-V2-01-U1072-AFB29ABB',          'V2',   '', 'Non-Deliverable', ''),
    (13, 'Library Output (SIGNAL)',    f'{PROJECT}-V3-LIBO-SIGNAL-ARM64-NOS-1V00', '1V00', '', 'Deliverable', ''),
    (14, 'Firmware Executable (SIG)',  f'{PROJECT}-V3-FWO-SIGNAL-ARM64-NOS-1V00',  '1V00', '', 'Non-Deliverable', ''),
    (15, 'Flash Executable (SIG)',     f'{PROJECT}-V3-FO-SIGNAL-ARM64-NOS-1V00',   '1V00', '', 'Non-Deliverable', ''),
    (16, 'FPGA Bitstream (SIG)',       f'{PROJECT}-V2-01-U10-DEE52D48',            'V2',   '', 'Non-Deliverable', ''),
    (17, 'Flash Loader',              f'{PROJECT}-V3-UO-FLDR-x86-NOS-1V00',       '1V00', '', 'Non-Deliverable', ''),
    (18, 'PIC Controller Firmware',   'DP-SPL-0208-V1-FWO-DP-SPL-5287-PIC18F-1V00', '1V00', '', 'Non-Deliverable', ''),
    (19, 'Library Source (ALGO)',      f'{PROJECT}-V3-LIBS-ALGO-ARM64-NOS-1V00',   '1V00', '', 'Deliverable', 'Algorithm Library with Platform Project'),
    (20, 'Firmware Source (ALGO)',     f'{PROJECT}-V3-FWS-ALGO-ARM64-NOS-1V00',    '1V00', '', 'Non-Deliverable', ''),
    (21, 'Flash Source (ALGO)',        f'{PROJECT}-V3-FS-ALGO-ARM64-NOS-1V00',     '1V00', '', 'Non-Deliverable', ''),
    (22, 'FPGA Source (ALGO)',         f'{PROJECT}-V2-01-GDS-U100-R1',             'V2',   '', 'Non-Deliverable', ''),
    (23, 'App Source (AUDIO)',         f'{PROJECT}-V3-AS-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Non-Deliverable', ''),
    (24, 'Flash Source (AUDIO)',       f'{PROJECT}-V3-FS-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Non-Deliverable', ''),
    (25, 'Driver Source (AUDIO)',      f'{PROJECT}-V3-DS-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Deliverable', ''),
    (26, 'OS Source (AUDIO)',          f'{PROJECT}-V3-OSS-AUDIO-ARM64-PLNX2019V2-1V00', '1V00', '', 'Deliverable', ''),
    (27, 'FPGA Source (AUDIO)',        f'{PROJECT}-V2-01-GDS-U99-R1',              'V2',   '', 'Non-Deliverable', ''),
    (28, 'Library Source (SIGNAL)',    f'{PROJECT}-V3-LIBS-SIGNAL-ARM64-NOS-1V00', '1V00', '', 'Deliverable', ''),
    (29, 'Firmware Source (SIGNAL)',   f'{PROJECT}-V3-FWS-SIGNAL-ARM64-NOS-1V00',  '1V00', '', 'Non-Deliverable', ''),
    (30, 'Flash Source (SIGNAL)',      f'{PROJECT}-V3-FS-SIGNAL-ARM64-NOS-1V00',   '1V00', '', 'Non-Deliverable', ''),
    (31, 'FPGA Source (SIG)',          f'{PROJECT}-V2-01-GDS-U25-R1',              'V2',   '', 'Non-Deliverable', ''),
    (32, 'LwIP Library Source',        f'{PROJECT}-V3-LIBS-LWIP_REPO-ARM64-NOS-1V00', '1V00', '', 'Deliverable', 'LwIP Repository'),
    (33, 'IPM Library Source',         f'{PROJECT}-V3-LIBS-IPM_REPO-ARM64-NOS-1V00', '1V00', '', 'Deliverable', 'Inter Processor Communication Repository'),
    (34, 'ADC Auto Test Executable',   f'{PROJECT}-V3-UO-ADCAUTO-X86-W7-1V00',    '1V00', '', 'Non-Deliverable', 'Board Level ADC Automated Test'),
    (35, 'IVI Shared Components',      f'{PROJECT}-V3-TOOLS-IVISC-X86-W7-1V00',    '1V00', '', 'Non-Deliverable', 'IVI drivers for instrument'),
    (36, 'SQL Schema Tools',           f'{PROJECT}-V3-TOOLS-SQLSCHEMA-NOS-1V00',   '1V00', '', 'Non-Deliverable', 'Database table and User Login Details'),
    (37, 'NIMAX Tools',               f'{PROJECT}-V3-TOOLS-NIMAX-X86-W7-1V00',    '1V00', '', 'Non-Deliverable', 'NIMAX for Instrument'),
    (38, 'MariaDB Tools',             f'{PROJECT}-V3-TOOLS-MARIADB-X86-W7-1V00',   '1V00', '', 'Non-Deliverable', 'MariaDB and ODBC Driver'),
    (39, 'Signal Gen Tools',          f'{PROJECT}-V3-TOOLS-SG-X86-W7-1V00',       '1V00', '', 'Non-Deliverable', 'IVI Driver for SigGen Instrument'),
    (40, 'ADC Auto Test Source',       f'{PROJECT}-V3-US- ADCAUTO-X86-W7-1V00',     '1V00', '', 'Non-Deliverable', ''),
    (41, 'Sample Applications',        f'{PROJECT}-V3-US-SAMPLES-1V00',             '1V00', '', 'Non-Deliverable', 'Sample Applications'),
]

# ── 1. CREATE CSV ──────────────────────────────────────────
def create_csv():
    header_html = (
        "<input type='button' value='Mail To CMO' class='btn btn-primary ewButton' "
        f"id='mailtocmo' onClick='mailtocmo({DOC_ID})'></input>"
        "<input type='button' value='Mail To Prj Incharge' class='btn btn-primary ewButton' "
        f"id='mailtoprjincharge' onClick='mailtoprjincharge({DOC_ID})'></input>"
        "<input type='button' value='Mail To Approver' class='btn btn-primary ewButton' "
        f"id='mailtoapprover' onClick='mailtoapprover({DOC_ID})'></input>"
        '"DOCUMENT ID"'
    )
    header_cols = [
        header_html, 'Sl No', 'CI Name', 'Control<br/>Category<br/>Level',
        'Fabricator Name', 'CI Reference', 'CI Version', 'CRQ#',
        'DELIVERABLE/<br/>NON-DELIVERABLE', 'Remarks', 'EMP Name,No',
        'Modified Dt', 'Document Link', 'Status', 'Approver', 'Approval Dt',
        'CMO Status', 'CMO Remarks', 'CMO Member', 'CMO Approval Dt'
    ]

    servers = ['itnas03', 'itnas04', '10.5.0.20']
    sw_dirs = [
        'SW\\SrcCode\\COMMS', 'SW\\SrcCode\\CTRL', 'SW\\SrcCode\\TOOLS',
        'SW\\VDD', 'SW\\SRS', 'SW\\SDD', 'HDD\\HRS', 'HDD\\TPR',
        'HDD\\TRP', 'HDD\\LDNG', 'SDG\\BUM',
    ]

    target_dir = os.path.join(DESKTOP, f'{PROJECT}_target_files')

    # Map CI reference → target subdirectory
    subdir_map = {
        'PWRSEQ': 'HW', 'CLKPRO': 'HW',
        'ALGO': 'ALGO', 'AUDIO': 'AUDIO', 'SIGNAL': 'SIGNAL',
        'TOOLS-SDK': 'TOOLS', 'TOOLS-IVISC': 'TOOLS',
        'TOOLS-MARIADB': 'TOOLS', 'TOOLS-NIMAX': 'TOOLS',
        'TOOLS-SG': 'TOOLS', 'TOOLS-SQLSCHEMA': 'TOOLS',
        'UO-FLDR': 'TOOLS', 'US-FLDR': 'TOOLS',
        'UO-ADCAUTO': 'TOOLS', 'US-ADCAUTO': 'TOOLS',
        'LWIP_REPO': 'TOOLS', 'IPM_REPO': 'TOOLS',
        'US-SAMPLES': 'TOOLS',
        'U105-': 'FPGA', 'U1072-': 'FPGA', 'U10-DEE': 'FPGA',
        'GDS-U100': 'FPGA', 'GDS-U99': 'FPGA', 'GDS-U25': 'FPGA',
        'DP-SPL-0208': 'PIC',
    }

    def get_target_path(ci_ref):
        """Resolve a CI reference to its actual Desktop target file path."""
        for key, subdir in subdir_map.items():
            if key in ci_ref:
                return os.path.join(target_dir, subdir, ci_ref + '.zip')
        # Fallback: put in root of target dir
        return os.path.join(target_dir, ci_ref + '.zip')

    csv_path = os.path.join(DESKTOP, 'ConfigItems_TST4455.csv')
    with open(csv_path, 'w', encoding='utf-8-sig', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(header_cols)

        for sl, name, ci_ref, ci_ver, crq, deliv, remarks in ci_entries:
            # Document Link points to actual Desktop target file
            doc_link = get_target_path(ci_ref)
            row = [
                DOC_ID, sl, name, '', '', ci_ref, ci_ver, crq, deliv, remarks,
                'Arun Prasad, 3310', '12/03/2026', doc_link,
                'APPROVED FOR LOADING', 'Rajesh Kumar, 5521', '15/03/2026',
                'APPROVED', '', 'Vikram Singh, 7842', '16/03/2026',
            ]
            writer.writerow(row)

    print(f'[OK] CSV created: {csv_path}')
    print(f'     Document Links point to: {target_dir}')
    return csv_path


# ── 2. CREATE ODT ──────────────────────────────────────────
def create_odt():
    """
    Build a minimal ODT file that looks like a VDD for DP-TST-4455.
    Contains tables with CI references and MD5 checksums.
    """
    import xml.etree.ElementTree as ET

    odt_path = os.path.join(DESKTOP, f'{PROJECT}-V3-VDD-1V00.odt')

    # We'll clone the existing ODT and modify text content
    src_odt = os.path.join(os.path.dirname(__file__), 'Input', 'DP-VPX-0209-V2-VDD-1V00.odt')
    tmp_dir = tempfile.mkdtemp(prefix='odt_build_')

    try:
        # Extract original ODT
        with zipfile.ZipFile(src_odt, 'r') as zin:
            zin.extractall(tmp_dir)

        # Read and modify content.xml
        content_path = os.path.join(tmp_dir, 'content.xml')
        with open(content_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # Replace project-specific strings
        replacements = {
            '6U Zynq MPSoC based Digital Receiver / Exciter with 4 RFADCs , 2 RFDACs &amp; Audio Processing for SDR platforms':
                '3U Zynq UltraScale+ based Secure Communications Processor with Dual RF Transceivers for Tactical SDR',
            'YIC2518-OPT_AIR': 'YIC3012-SDR_COMMS',
            'Balamurali/Sapthagiri/Mouli Shanmugam': 'Test Engineer One/Test Engineer Two',
            'Sapthagiri P, 2177': 'Arun Prasad, 3310',
            'Dhayanandhan R, 895': 'Vikram Singh, 7842',
            'Hafiz Haja, 1869': 'Priya Sharma, 4201',
            'Ramesh M, 606': 'Amit Patel, 3817',
            'Sathishkumar K, 450': 'Rajesh Kumar, 5521',
            'Shanmugam S': 'Review Engineer Alpha',
            'Sathishkumar K': 'Approver Engineer Beta',
            'DP-VPX-0209-V2': f'{PROJECT}-V3',
            'DP-VPX-0209': PROJECT,
            'Sept 08, 2022': 'March 15, 2026',
            'Sept 28,2022': 'March 20, 2026',
            'July 8, 2022': 'February 10, 2026',
            '2.00': '3.00',
        }

        # Apply replacements in order (longer strings first to avoid partial matches)
        for old, new in sorted(replacements.items(), key=lambda x: -len(x[0])):
            content = content.replace(old, new)

        # Replace MD5 checksums with random ones
        import re
        def random_md5():
            return ''.join(random.choices('0123456789ABCDEF', k=32))

        def random_hex8():
            return ''.join(random.choices('0123456789ABCDEF', k=8))

        # Replace 32-char hex (MD5) — only outside XML tags
        def replace_hex32(match):
            return random_md5()
        content = re.sub(r'(?<=>)([0-9A-Fa-f]{32})(?=<)', replace_hex32, content)
        # Replace 8-char hex checksums — only in text content
        content = re.sub(r'(?<=>)([0-9A-Fa-f]{8})(?=<)', lambda m: random_hex8(), content)

        # XML-aware replacement: handle project name split across <text:span> elements
        def xml_aware_replace(content, old_str, new_str):
            """Replace old_str with new_str, allowing XML tags between any characters."""
            pattern_parts = []
            for ch in old_str:
                pattern_parts.append(re.escape(ch) + r'(?:</[^>]*>)*(?:<[^>]*>)*')
            pattern = ''.join(pattern_parts)
            return re.sub(pattern, new_str, content)

        # Apply XML-aware replacement for the project name
        content = xml_aware_replace(content, 'DP-VPX-0209', PROJECT)
        content = xml_aware_replace(content, 'Balamurali', 'TestEngineer1')
        content = xml_aware_replace(content, 'Sapthagiri', 'TestEngineer2')
        content = xml_aware_replace(content, 'Mouli Shanmugam', 'TestEngineer3')

        with open(content_path, 'w', encoding='utf-8') as f:
            f.write(content)

        # Also update meta.xml
        meta_path = os.path.join(tmp_dir, 'meta.xml')
        if os.path.exists(meta_path):
            with open(meta_path, 'r', encoding='utf-8') as f:
                meta = f.read()
            meta = meta.replace('DP-VPX-0209', PROJECT)
            meta = meta.replace('V2', 'V3')
            with open(meta_path, 'w', encoding='utf-8') as f:
                f.write(meta)

        # Repackage as ODT
        with zipfile.ZipFile(odt_path, 'w', zipfile.ZIP_DEFLATED) as zout:
            for root, dirs, files in os.walk(tmp_dir):
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.relpath(file_path, tmp_dir)
                    zout.write(file_path, arcname)

    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)

    print(f'[OK] ODT created: {odt_path}')
    return odt_path


# ── 3. CREATE DUMMY TARGET FILES ───────────────────────────
def create_target_files():
    """
    Create a target directory on Desktop with dummy files matching CI references.
    The Checksum app verifies files in a target directory against the VDD/CSV.
    """
    target_dir = os.path.join(DESKTOP, f'{PROJECT}_target_files')
    os.makedirs(target_dir, exist_ok=True)

    subdirs = ['HW', 'ALGO', 'AUDIO', 'SIGNAL', 'TOOLS', 'FPGA', 'PIC']
    for sd in subdirs:
        os.makedirs(os.path.join(target_dir, sd), exist_ok=True)

    dummy_files = [
        ('HW',     f'{PROJECT}-000-PWRSEQ-1V00.zip'),
        ('HW',     f'{PROJECT}-000-CLKPRO-1V00.zip'),
        ('ALGO',   f'{PROJECT}-V3-LIBO-ALGO-ARM64-NOS-1V00.zip'),
        ('ALGO',   f'{PROJECT}-V3-FWO-ALGO-ARM64-NOS-1V00.zip'),
        ('ALGO',   f'{PROJECT}-V3-FO-ALGO-ARM64-NOS-1V00.zip'),
        ('FPGA',   f'{PROJECT}-V2-01-U105-032D07D0.zip'),
        ('AUDIO',  f'{PROJECT}-V3-AO-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('AUDIO',  f'{PROJECT}-V3-FO-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('AUDIO',  f'{PROJECT}-V3-DO-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('AUDIO',  f'{PROJECT}-V3-OSO-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-TOOLS-SDK-ARM64-PLNX2019V2-1V00.zip'),
        ('FPGA',   f'{PROJECT}-V2-01-U1072-AFB29ABB.zip'),
        ('SIGNAL', f'{PROJECT}-V3-LIBO-SIGNAL-ARM64-NOS-1V00.zip'),
        ('SIGNAL', f'{PROJECT}-V3-FWO-SIGNAL-ARM64-NOS-1V00.zip'),
        ('SIGNAL', f'{PROJECT}-V3-FO-SIGNAL-ARM64-NOS-1V00.zip'),
        ('FPGA',   f'{PROJECT}-V2-01-U10-DEE52D48.zip'),
        ('TOOLS',  f'{PROJECT}-V3-UO-FLDR-x86-NOS-1V00.zip'),
        ('PIC',    'DP-SPL-0208-V1-FWO-DP-SPL-5287-PIC18F-1V00.zip'),
        ('ALGO',   f'{PROJECT}-V3-LIBS-ALGO-ARM64-NOS-1V00.zip'),
        ('ALGO',   f'{PROJECT}-V3-FWS-ALGO-ARM64-NOS-1V00.zip'),
        ('ALGO',   f'{PROJECT}-V3-FS-ALGO-ARM64-NOS-1V00.zip'),
        ('FPGA',   f'{PROJECT}-V2-01-GDS-U100-R1.zip'),
        ('AUDIO',  f'{PROJECT}-V3-AS-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('AUDIO',  f'{PROJECT}-V3-FS-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('AUDIO',  f'{PROJECT}-V3-DS-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('AUDIO',  f'{PROJECT}-V3-OSS-AUDIO-ARM64-PLNX2019V2-1V00.zip'),
        ('FPGA',   f'{PROJECT}-V2-01-GDS-U99-R1.zip'),
        ('SIGNAL', f'{PROJECT}-V3-LIBS-SIGNAL-ARM64-NOS-1V00.zip'),
        ('SIGNAL', f'{PROJECT}-V3-FWS-SIGNAL-ARM64-NOS-1V00.zip'),
        ('SIGNAL', f'{PROJECT}-V3-FS-SIGNAL-ARM64-NOS-1V00.zip'),
        ('FPGA',   f'{PROJECT}-V2-01-GDS-U25-R1.zip'),
        ('TOOLS',  f'{PROJECT}-V3-LIBS-LWIP_REPO-ARM64-NOS-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-LIBS-IPM_REPO-ARM64-NOS-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-UO-ADCAUTO-X86-W7-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-TOOLS-IVISC-X86-W7-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-TOOLS-SQLSCHEMA-NOS-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-TOOLS-NIMAX-X86-W7-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-TOOLS-MARIADB-X86-W7-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-TOOLS-SG-X86-W7-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-US- ADCAUTO-X86-W7-1V00.zip'),
        ('TOOLS',  f'{PROJECT}-V3-US-SAMPLES-1V00.zip'),
    ]

    for subdir, fname in dummy_files:
        fpath = os.path.join(target_dir, subdir, fname)
        # Write random bytes so each file has a unique hash
        with open(fpath, 'wb') as f:
            f.write(os.urandom(random.randint(256, 2048)))

    print(f'[OK] Target files created: {target_dir}')
    print(f'     {len(dummy_files)} dummy files in {len(subdirs)} subdirectories')
    return target_dir


# ── MAIN ────────────────────────────────────────────────────
if __name__ == '__main__':
    print(f'=== Creating test files for project {PROJECT} ===')
    print(f'Desktop: {DESKTOP}')
    print()

    csv_path = create_csv()
    odt_path = create_odt()
    target_dir = create_target_files()

    print()
    print('=== SUMMARY ===')
    print(f'  CSV:  {csv_path}')
    print(f'  ODT:  {odt_path}')
    print(f'  Target dir: {target_dir}')
    print()
    print('Use these files to test the VDD Auditor:')
    print(f'  1. Import CSV: {os.path.basename(csv_path)}')
    print(f'  2. Import ODT: {os.path.basename(odt_path)}')
    print(f'  3. Target directory: {target_dir}')
