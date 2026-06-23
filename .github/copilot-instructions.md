# GitHub Copilot Instructions

These instructions guide GitHub Copilot and other AI coding agents when working
in this repository.

## Cherry-picking commits

When cherry-picking a change (for example, backporting a fix to a release
branch):

1. **Cherry-pick commits, not PRs.** Identify the specific commit SHA to
   cherry-pick. If the source PR was squash-merged, that is the single squashed
   commit produced on the target branch.
2. **Use `git cherry-pick -x`** so the new commit message records the source
   commit in the standard git format:

   ```
   (cherry picked from commit <sha>)
   ```

3. **Create a topic branch** off the destination branch and apply the
   cherry-pick there.
4. **Open the PR** with:
   - **Title:** start with `[Cherry-Pick]`, followed by the cherry-picked
     commit's subject line. For example:

     ```
     [Cherry-Pick] <original commit subject>
     ```

   - **Description:** the cherry-picked commit's message body verbatim,
     including the trailing `(cherry picked from commit <sha>)` line. Because
     PRs are squash-merged, the PR description becomes the merge commit message,
     so it must match the cherry-picked commit message.

### Example

Title:

```
[Cherry-Pick] Fix cast between semantically different integer types (#8414)
```

Description:

```
This patch fixes 2 instances of internal issues; C6214: Cast between
semantically different integer types.

(cherry picked from commit 32beebfd584c59662392779184240c6d407d5346)
```
