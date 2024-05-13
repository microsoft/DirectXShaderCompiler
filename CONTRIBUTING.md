# How to contribute

One of the easiest ways to contribute is to participate in discussions and discuss issues. You can also contribute by submitting pull requests with code changes.


## General feedback and discussions?
Please start a discussion on the repo issue tracker.


## Bugs and feature requests?
For non-security related bugs please log a new issue in the GitHub repo.


## Reporting security issues and bugs
Security issues and bugs should be reported privately, via email, to the Microsoft Security Response Center (MSRC) secure@microsoft.com. You should receive a response within 24 hours. If for some reason you do not, please follow up via email to ensure we received your original message. Further information, including the MSRC PGP key, can be found in the [Security TechCenter](https://technet.microsoft.com/en-us/security/ff852094.aspx).


## Filing issues
When filing issues, please use our [bug filing templates](https://github.com/aspnet/Home/wiki/Functional-bug-template).
The best way to get your bug fixed is to be as detailed as you can be about the problem.
Providing a minimal project with steps to reproduce the problem is ideal.
Here are questions you can answer before you file a bug to make sure you're not missing any important information.

1. Did you read the documentation?
2. Did you include the snippet of broken code in the issue?
3. What are the *EXACT* steps to reproduce this problem?
4. What version are you using?

GitHub supports [markdown](https://help.github.com/articles/github-flavored-markdown/), so when filing bugs make sure you check the formatting before clicking submit.


## Contributing code and content
You will need to complete a Contributor License Agreement (CLA) before your pull request can be accepted. This agreement testifies that you are granting us permission to use the source code you are submitting, and that this work is being submitted under appropriate license that we can use it.

You can complete the CLA by going through the steps at the [Contribution License Agreement site](https://cla.microsoft.com). Once we have received the signed CLA, we'll review the request. You will only need to do this once.

Make sure you can build the code. Familiarize yourself with the project workflow and our coding conventions. If you don't know what a pull request is read this article: https://help.github.com/articles/using-pull-requests. The hcttest command is your friend.

Before submitting a feature or substantial code contribution please discuss it with the team and ensure it follows the product roadmap. You might also read these two blogs posts on contributing code: [Open Source Contribution Etiquette](http://tirania.org/blog/archive/2010/Dec-31.html) by Miguel de Icaza and [Don't "Push" Your Pull Requests](https://www.igvita.com/2011/12/19/dont-push-your-pull-requests/) by Ilya Grigorik. Note that all code submissions will be rigorously reviewed and tested by the team, and only those that meet an extremely high bar for both quality and design/roadmap appropriateness will be merged into the source.

Here's a few things you should always do when making changes to the code base:

**Engineering guidelines**

The coding, style, and general engineering guidelines follow those described in the docs/CodingStandards.rst. For additional guidelines in code specific to HLSL, see the docs/HLSLChanges.rst file.

DXC has adopted a clang-format requirement for all incoming changes. PRs to DXC should have the _changed code_ clang formatted to the LLVM style, and leave the remaining portions of the file unchanged. This can be done using the `git-clang-format` tool or IDE driven workflows. A GitHub action will run on all PRs to validate that the change is properly formatted.

**Commit/Pull Request Format**

```
Summary of the changes (Less than 80 chars)
 - Detail 1
 - Detail 2

Fixes #bugnumber (in this specific format)
```

Your pull request should:

* Include a description of what your change intends to do
  * The title of your PR should be a very brief description of the change, and
    can use tags to allow for speedy categorization.
    * Titles under 76 characters print nicely in unix terminals under `git log`.
      This is not a hard requirement, but is good guidance.
    * Title tags are generally one word or acronym enclosed in square brackets.
      Limiting to one or two tags is ideal to keep titles short. Some examples
      of common tags are:
      * [NFC] - No Functional Change
      * [RFC] - Request For Comments (often used for drafts to get feedback)
      * [Doc] - Documentation change
      * [SPIRV] - Changes related to SPIR-V
      * [HLSL2021] - Changes related to HLSL 2021 features
      * Other tags in use: [Linux], [mac], [Win], [PIX], etc...
      * Tags aren't formalized or any specific limited set. If you're unsure of
        a reasonable tag to use, just don't use any. If you want to invent a new
        tag, go for it! These are to help categorize changes at a glance.
  * The PR description should include a more detailed description of the change,
    an explanation for the motivation of the change, and links to any relevant
    Issues. This does not need to be a dissertation, but should leave
    breadcrumbs for the next person debugging your code (who might be you).
  * Using the words `Fixes`, `Fixed`, `Closes`, `Closed`, or `Close` followed by
    `#<issuenumber>`, will auto close an issue after the PR is merged.
* Be a child commit of a reasonably recent commit in the main branch
* Pass all unit tests
* Ensure that the title and description are fully up to date before merging
  * The title and description feed the final git commit message, and we want to
    ensure high quality commit messages in the repository history.
* Include adequate tests
  * At least one test should fail in the absence of your non-test code changes. If your PR does not match this criteria, please specify why
  * Tests should include reasonable permutations of the target fix/change
  * Include baseline changes with your change as needed
