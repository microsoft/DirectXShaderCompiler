---
name: cherry-pick
description: Cherry-pick a commit onto another branch and open the PR. Use when the user asks to cherry-pick or backport a commit or PR to a release branch.
---

# Cherry-picking commits

Use this when asked to cherry-pick or backport a change.

1. Identify the source commit SHA to cherry-pick. If the source PR was
   squash-merged, the single squashed commit on the target branch is what you
   cherry-pick.
2. Create a topic branch off the destination branch.
3. Apply the change with `git cherry-pick -x <sha>` so the commit message
   records the source commit in git's standard format:

   ```
   (cherry picked from commit <sha>)
   ```

   Keep the cherry-pick commit message as git generates it by default.
4. Open the PR with the title prefixed `[Cherry-Pick]`, followed by the
   original commit subject.
