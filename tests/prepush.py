#!/usr/bin/env python3

import os
import sys
import subprocess

scriptPath = os.path.abspath(__file__)
if os.path.islink(scriptPath):
    scriptPath = os.path.realpath(scriptPath)
scriptDir = os.path.dirname(scriptPath)
repoRoot = os.path.dirname(scriptDir)
sys.path.insert(0, scriptDir)

historyPath = os.path.join(scriptDir, "test_history.json")
pendingPath = os.path.join(scriptDir, "test_history_pending.json")

from history_utils import parse_results_json, load_history, save_history, calculate_stats_from_content

def run_git(args):
    result = subprocess.run(["git"] + args, capture_output=True, text=True, cwd=repoRoot)
    return result.stdout.strip()

def get_file_at_commit(commit, path):
    result = subprocess.run(["git", "show", f"{commit}:{path}"], capture_output=True, text=True, cwd=repoRoot)
    if result.returncode != 0:
        return None
    return result.stdout

def get_commits(rev_range):
    output = run_git(["log", "--format=%H|%s|%ai", rev_range, "--reverse", "--", "tests/current_results.json"])
    commits = []
    for line in output.strip().split("\n"):
        if line:
            parts = line.split("|", 2)
            if len(parts) == 3:
                commits.append({"hash": parts[0], "message": parts[1], "date": parts[2]})
    return commits

def calculate_stats_at_commit(commit_hash):
    results_content = get_file_at_commit(commit_hash, "tests/current_results.json")
    if not results_content:
        return None, None
    try:
        results = parse_results_json(results_content)
    except:
        return None, None
    def get_dump(version, name):
        return get_file_at_commit(commit_hash, f"tests/dumps/{version}/{name}.json")
    return calculate_stats_from_content(results, get_dump)

def get_upstream_commit():
    result = subprocess.run(["git", "rev-parse", "@{u}"], capture_output=True, text=True, cwd=repoRoot)
    if result.returncode != 0:
        return None
    return result.stdout.strip()

history_data = load_history(historyPath)

if not history_data:
    print("No history file found, regenerating from all commits...")
    output = run_git(["log", "--format=%H|%s|%ai", "--reverse", "--", "tests/current_results.json"])
    commits = []
    for line in output.strip().split("\n"):
        if line:
            parts = line.split("|", 2)
            if len(parts) == 3:
                commits.append({"hash": parts[0], "message": parts[1], "date": parts[2]})
else:
    upstream_hash = get_upstream_commit()
    if upstream_hash:
        truncate_idx = len(history_data)
        for i, entry in enumerate(history_data):
            if entry["fullHash"] == upstream_hash:
                truncate_idx = i + 1
                break
        if truncate_idx < len(history_data):
            print(f"Truncating {len(history_data) - truncate_idx} entries after upstream")
            history_data = history_data[:truncate_idx]
    commits = get_commits("@{u}..HEAD")

if not commits:
    exit(0)

for commit in commits:
    total_errors, total_frames = calculate_stats_at_commit(commit["hash"])
    if total_errors is None or total_frames is None or total_frames == 0:
        continue

    if history_data and history_data[-1]["errorCount"] == total_errors and history_data[-1]["frameCount"] == total_frames:
        continue

    history_data.append({
        "hash": commit["hash"][:8],
        "fullHash": commit["hash"],
        "message": commit["message"],
        "date": commit["date"],
        "errorCount": total_errors,
        "frameCount": total_frames,
        "errorPercent": round(100.0 * total_errors / total_frames, 4)
    })
    print(f"Added {commit['hash'][:8]}: {commit['message'][:50]}")

save_history(historyPath, history_data)
