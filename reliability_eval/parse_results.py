import os
import re


FAULT_MAP = {
    0: ["SBE", "1 bit (SBE)"],
    1: ["PIN_1", "1 pin (PIN_1)"],
    2: ["SCE", "1 chip (SCE)"],
    3: ["DBE", "2 x 1 bit (DBE)"],
    4: ["TBE", "3 x 1 bit (TBE)"],
    5: ["SCE_SBE", "1 chip + 1 bit"],
    6: ["SCE_DBE", "1 chip + 2 bits"],
    7: ["SCE_SCE", "1 chip + 1 chip"],
    8: ["RANK", "rank"]
}

RECC_MAP = {
    1: "AMDCHIPKILL",
    2: "OOC",
    3: "QOC"
}

fault_params = [0, 1, 2, 3, 4, 5, 6, 7, 8]
recc_params = [1, 2, 3]

# --- Script Body ---

def parse_result_file(filepath):
    """Parses the CE, DUE, and SDC rates from a given result file."""
    results = {'CE': 0.0, 'DUE': 0.0, 'SDC': 0.0}
    try:
        with open(filepath, 'r') as f:
            content = f.read()
            ce_match = re.search(r"CE\s*:\s*([0-9\.]+)", content)
            due_match = re.search(r"DUE\s*:\s*([0-9\.]+)", content)
            sdc_match = re.search(r"SDC\s*:\s*([0-9\.]+)", content)

            if ce_match:
                results['CE'] = float(ce_match.group(1))
            if due_match:
                results['DUE'] = float(due_match.group(1))
            if sdc_match:
                results['SDC'] = float(sdc_match.group(1))

    except FileNotFoundError:
        print(f"Warning: Result file not found at '{filepath}'")
    except Exception as e:
        print(f"Error parsing file '{filepath}': {e}")
        
    return results

def main():
    """Main execution function"""
    all_results = {}

    # Read files and parse data based on the defined parameters
    for fault_p in fault_params:
        fault_info = FAULT_MAP.get(fault_p)
        if not fault_info:
            continue
        
        fault_filename_str, fault_report_name = fault_info
        all_results[fault_report_name] = {}

        for recc_p in recc_params:
            recc_name = RECC_MAP.get(recc_p)
            if not recc_name:
                continue

            # File naming convention from C++ code: RECC + "_" + FAULT + ".S"
            filename = f"{recc_name}_{fault_filename_str}.S"
            
            parsed_data = parse_result_file(filename)
            all_results[fault_report_name][recc_name] = parsed_data

    # --- Print Final Result Table ---
    recc_names_ordered = [RECC_MAP[key] for key in sorted(RECC_MAP.keys()) if key in recc_params]
    
    header = f"{'Error scenario':<22}"
    for name in recc_names_ordered:
        header += f"{name.replace('_', ' '):>18}"
    print(header)
    print("=" * len(header))

    fault_names_ordered = [FAULT_MAP[key][1] for key in sorted(FAULT_MAP.keys()) if key in fault_params]

    for fault_name in fault_names_ordered:
        for metric in ['CE', 'DUE', 'SDC']:
            scenario_label = fault_name if metric == 'CE' else ''
            line = f"{scenario_label:<18} {metric+' (%)':<4}"
            
            for recc_name in recc_names_ordered:
                value = all_results.get(fault_name, {}).get(recc_name, {}).get(metric, 0.0) * 100
                line += f"{value:>18.4f}"
            
            print(line)
        print("-" * len(header))


if __name__ == "__main__":
    main()