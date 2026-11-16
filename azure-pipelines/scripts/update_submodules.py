#!/usr/bin/env python3
"""Moves external submodules to their latest upstream commit with green CI and
opens a pull request, or refreshes the one already open."""

import argparse
import json
import subprocess
from dataclasses import dataclass

PASSING_CHECK_CONCLUSIONS = {"success", "neutral", "skipped"}


@dataclass
class SubmoduleUpdate:
    name: str
    new_sha: str
    compare_url: str
    commit_count: int
    subjects: list[str]

    def describe(self):
        header = f"* [{self.name}]({self.compare_url}) ({self.commit_count} commits):"
        return "\n".join([header] + [f"  > {subject}" for subject in self.subjects])


def run(*args):
    return subprocess.check_output(args, encoding="utf-8").strip()


def gh_api(path):
    return json.loads(run("gh", "api", path))


def github_repo(url):
    return url.removeprefix("https://github.com/").removesuffix(".git")


def is_green(repo, sha):
    check_runs = gh_api(f"repos/{repo}/commits/{sha}/check-runs?per_page=100")
    statuses = gh_api(f"repos/{repo}/commits/{sha}/status")
    checks_passed = all(
        check["conclusion"] in PASSING_CHECK_CONCLUSIONS
        for check in check_runs["check_runs"]
    )
    statuses_passed = all(
        status["state"] == "success" for status in statuses["statuses"]
    )
    has_ci = check_runs["total_count"] + statuses["total_count"] > 0
    return has_ci and checks_passed and statuses_passed


def update_submodule(path, upstream_branch):
    url = run("git", "config", "-f", ".gitmodules", "--get", f"submodule.{path}.url")
    repo = github_repo(url)
    old_sha = run("git", "rev-parse", f"HEAD:{path}")
    new_sha = gh_api(f"repos/{repo}/commits/{upstream_branch}")["sha"]

    if old_sha == new_sha:
        print(f"{path} is up to date at {old_sha}.")
        return None
    if not is_green(repo, new_sha):
        print(f"Skipping {path}: CI for {repo}@{new_sha} is not green.")
        return None

    run("git", "update-index", "--cacheinfo", f"160000,{new_sha},{path}")
    comparison = gh_api(f"repos/{repo}/compare/{old_sha}...{new_sha}")
    return SubmoduleUpdate(
        name=path.split("/")[-1],
        new_sha=new_sha,
        compare_url=comparison["html_url"],
        commit_count=comparison["total_commits"],
        subjects=[
            c["commit"]["message"].splitlines()[0] for c in comparison["commits"]
        ],
    )


def configure_git_identity():
    user = gh_api("user")
    email = f"{user['id']}+{user['login']}@users.noreply.github.com"
    run("git", "config", "user.name", user["login"])
    run("git", "config", "user.email", email)
    run("gh", "auth", "setup-git")


def open_or_refresh_pull_request(repo, base_branch, update_branch, title, body):
    owner = repo.split("/")[0]
    open_prs = gh_api(f"repos/{repo}/pulls?state=open&head={owner}:{update_branch}")
    pr_fields = ["--repo", repo, "--title", title, "--body", body]
    if open_prs:
        print(run("gh", "pr", "edit", str(open_prs[0]["number"]), *pr_fields))
    else:
        branches = ["--base", base_branch, "--head", update_branch]
        print(run("gh", "pr", "create", *branches, *pr_fields))


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default="microsoft/DirectXShaderCompiler")
    parser.add_argument("--base-branch", default="main")
    parser.add_argument("--update-branch", default="automation/update-submodules")
    parser.add_argument("--upstream-branch", default="main")
    parser.add_argument(
        "--submodules",
        nargs="+",
        default=[
            "external/DirectX-Headers",
            "external/SPIRV-Headers",
            "external/SPIRV-Tools",
        ],
    )
    return parser.parse_args()


def main():
    args = parse_args()
    configure_git_identity()
    run("git", "fetch", "origin", args.base_branch)
    run("git", "checkout", "-B", args.update_branch, "FETCH_HEAD")

    updates = [
        update
        for path in args.submodules
        if (update := update_submodule(path, args.upstream_branch))
    ]
    if not updates:
        print("No submodule updates to propose.")
        return

    title = "Update submodules: " + ", ".join(
        f"{update.name} to {update.new_sha[:8]}" for update in updates
    )
    body = "\n".join(update.describe() for update in updates)

    run("git", "commit", "-m", title, "-m", body)
    run("git", "push", "--force", "origin", f"HEAD:refs/heads/{args.update_branch}")
    open_or_refresh_pull_request(
        args.repo, args.base_branch, args.update_branch, title, body
    )


if __name__ == "__main__":
    main()
