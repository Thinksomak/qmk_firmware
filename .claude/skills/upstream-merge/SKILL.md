---
description: Merge latest upstream QMK master into the current branch and rebuild all boards.
disable-model-invocation: true
---

1. Run `git status`. Stop if the tree is dirty and ask what to do.
2. `git fetch upstream`, then `git rev-list --left-right --count HEAD...upstream/master` and report ahead/behind.
3. If behind: `git merge upstream/master`. If conflicts appear, list the files and stop. Do not resolve board JSON conflicts without asking.
4. Run the `verify` skill for ALL boards.
5. Check `git submodule status | grep -E '^[+-]'`. Report any out-of-sync submodule.
6. Known validator rules: keyboard.json needs a non-empty `url`; never keep info.json and keyboard.json together. Fix these only after confirming with me.
7. Do not push. Show the summary and wait.