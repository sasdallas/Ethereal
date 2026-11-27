# Ethereal's AI policy

**This AI policy is subject to change at the discretion of the developer at any time.**

## TL;DR

Ethereal does not permit PRs containing large chunks of AI generated code (anything more than minor fixes).

**PRs containing AI-generated or suspected AI-generated code can be closed at any time.**

**PRs that have used AI tools in any way MUST acknowledge this in their description.**

**There is no vibecoding in Ethereal currently, nor will there ever be.**

**Suspicions of AI-generated code? Start an issue in GitHub issues for an explanation.**\
I make mistakes, and sometimes a late night push gets a test file sent to `main`. Doesn't mean I'm a vibecoder or I'm lying in this policy.

Note that the above does not apply to external projects.

## Overview (+ my thoughts)

First and foremost, I do not intend to criticize, call out, or in any other way attack AI users as
there is plenty of that on the internet already.

I'm not against vibecoding as a concept of allowing those more unfamiliar with programming
to become better, however I am very against its usage in modern society (i.e. for monetary gain, "slop", etc.).

Artificial intelligence is a tool that is here to stay, but that does **not** mean that it is the future!

As I have noticed (from myself and others) people tend to "overdose" on AI and become dependent on it.\
This only hurts you in the long run, causing your ability to program to falter.

For your own sake, please avoid using AI in the wrong ways.

## What's in Ethereal right now

Ethereal's code is near-entirely human written (referring to code contained directly in repository).\

As far as I know, all code contained in this repository right now has no traces of remaining AI, though it may have been used
for prototyping in the past. 

**External projects (yes, this even includes my other projects submoduled in) may have AI code within them, and I release responsibility on those**

## Permissions

Artificial intelligence is **permitted** for:

- Local experimentations that are not pushed to master
- Locating bugs within the kernel given a list of symptoms
- Writing small bugfixes for bugs (i.e., ideally single line or single block of code, full design flaws must be evaluated by developer).
- Reading through large codebases (assuming a human double-checks) such as Linux or NetBSD.
- Explaining foreign concepts (when this information is harder to access than say, a google search or whatever)
- Generating repetitive files
  - Clarification: By repetitive, this would mean like lists of sources, repetitive build scripts, header file definitions, etc.
    - It is up to the repository owner's discretion to choose what is repetitive or not.
    - In most cases, this case in Ethereal has only been used a couple of times and it's a clause I don't like to invoke.
- Prototyping code/designs
  - Ideally, one should avoid using AI to fully create a design, as most of its designs are overengineered and just bad.
  - Instead, using AI to suggest fixes/pitfalls to designs is a great idea.
  - AI code should be replaced or at least rewritten, like come on I don't need slop.
- Code review existing code

Artificial intelligence is **disallowed** for:

- Vibecoding
  - If you clearly do not understand the code it has written, I suggest you don't PR it and waste my time.
  - "Vibecoding" is allowed for local experimentation only.
- Image/asset generation
- Writing documentation
- Generating designs
- Basically anything that doesn't fall under permitted.

The developer reserves the rights to expand either of these clauses at will.

**PRs containing AI-generated or suspected AI-generated code can be closed at any time.**

**PRs that have used AI tools in any way MUST acknowledge this in their description.**
