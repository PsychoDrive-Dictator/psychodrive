import json
import os

SKIP_ERROR_TYPES = {1, 2, 3, 4}

def parse_results_json(content):
    content = content.replace("var currentResults =", "").replace("var newResults =", "").strip()
    if content.endswith(";"):
        content = content[:-1]
    return json.loads(content)

def load_history(path):
    if not os.path.exists(path):
        return []
    with open(path) as f:
        content = f.read()
    content = content.replace("var historyData =", "").replace("var pendingEntry =", "").strip()
    if content.endswith(";"):
        content = content[:-1]
    return json.loads(content)

def save_history(path, data, var_name="historyData"):
    with open(path, "w") as f:
        f.write("var " + var_name + " =\n" + json.dumps(data, indent=2) + ";\n")

def get_error_count(results):
    total_errors = 0
    for r in results:
        for i, error_type in enumerate(r.get("errorTypes", [])):
            if i not in SKIP_ERROR_TYPES:
                total_errors += error_type.get("count", 0)

    return total_errors

def get_frame_count(results, dumps_dir):
    total_frames = 0
    for r in results:
        test_name = r["testName"]
        test_version = r["testVersion"]
        test_path = os.path.join(dumps_dir, test_version, f"{test_name}.json")
        if os.path.exists(test_path):
            with open(test_path) as f:
                frames = json.loads(f.read())
                total_frames += len(frames)

    return total_frames

def calculate_stats(results, dumps_dir):
    total_errors = get_error_count(results)
    total_frames = get_frame_count(results, dumps_dir)

    return total_errors, total_frames

def calculate_stats_from_content(results, get_dump_content):
    total_errors = 0
    for r in results:
        for i, error_type in enumerate(r.get("errorTypes", [])):
            if i not in SKIP_ERROR_TYPES:
                total_errors += error_type.get("count", 0)

    total_frames = 0
    for r in results:
        test_name = r["testName"]
        test_version = r["testVersion"]
        dump_content = get_dump_content(test_version, test_name)
        if dump_content:
            try:
                frames = json.loads(dump_content)
                total_frames += len(frames)
            except json.JSONDecodeError:
                pass

    return total_errors, total_frames
