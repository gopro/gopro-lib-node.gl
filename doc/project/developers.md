Developer guidelines
====================

## Pull-request checklist

### Style/cosmetics

Cosmetics inconsistencies and cruft are the first things we notice, adding
noise and distraction during the review, and often causing unnecessary review
iterations. Make sure it's neat and clean before we can tackle more important
topics:

- Is your **coding style** following exactly what you observe within the files
  you're modifying?
- Do you have any **trailing whitespaces** or **missing end of file line
  break**? (this is usually spotted by Git or a well configured editor)
- Make sure there is **no stray change** unrelated to your commits
- Whitespace/indent/typo/style/etc issue fixes belong in **dedicated commits**.

### Functionality

Some simple checks to greatly improve the basic functionality of your
submission:

- Make sure the build and **tests pass for every commits**: `git rebase master
  --exec "make clean && make test"`
- Can the same changes be **applied elsewhere** in the code base?
- Avoid at all cost **glob-like code** such as `import *` or `*.ext` file
  references: this kind of code leads to dangling references because they break
  the obvious links between components
- Double check **anything that can reference** the part you're making
  modifications to (including the documentation); `git grep -i` on the file
  names, symbols and keywords is your friend
- Check the **imports**, **includes**, **dependencies**: are they still needed
  after your changes?
- Do you check and raise all possible **errors**?
- Have you checked for potential **memory leaks**, in particular in error code
  paths?
- Do you **assert in impossible code paths** to help the compilers, tools and
  developers make assumptions about the code?
- Are the files you changed now in a **better state** than they were before?
- Make sure you **understand deeply** and can **explain** every changes

### Design

Now is the time to take a step back:

- Have you considered the impact of your changes from a **user perspective**?
- Can your changes be made **simpler**? [Keep It Simple, Stupid][kiss]
- Try to **avoid external dependencies**, especially for core features
- If you are altering the **architecture and internal dependencies** between
  components, is it clarifying / following the current structure or entangling
  it?
- What impact your changes have on the **maintainability**?

### Exposure

In order to keep the API maintainable in the long term, we try to keep it as
small as possible. This means the only public header of the project `nodegl.h`
must be kept with as little symbols as possible. Additionally, we try to
prevent painful integration for our users:

- Do not reorder public enum such as `NGL_PLATFORM_*` as it breaks ABI
- Do not insert a value in the middle of a public enum as it breaks ABI
- Do not remove or change a public prototype as it breaks API and ABI

### Integration

This part is where we focus on the finalization of the work to reach production
quality:

- Are your commits **atomics**?
- Check that your commit messages use a **standard prefix** (look at the Git
  history for the commonly used ones)
- Do your commit descriptions explain the context of **why** you're making each
  change for someone unfamiliar with the topic?
- Spend some time re-reading your **commit messages and descriptions**: [How to
  write a Git commit message][git-commit-msg]
- If you're adding code, is it covered by **tests**?
- If you're fixing a bug, do you have a **test** or some **safeguard** to
  prevent it from being re-introduced in the future / another context?
- If the code is a workaround for an issue outside the control of the project,
  did you **link to the issue** with a comment (and opened it if needed) in the
  code?
- If a hack/shortcut is considered unavoidable, is it **documented** in the
  code about the why and what could be done about it in the future?
- **No merge commit** is allowed in the history, make sure you learn how to use
  Git and the `git rebase` command in particular
- **Big binary files** (as in > 200kB) should be avoided in the repository


## Review Process

If your code passes the check list, you can submit a Pull Request for the
`master` branch on Github.  Reviewers will likely make comments and request for
changes. You are expected to discuss your changes, and rework the history as
needed until it reaches a consensus.

When you've addressed the requested changes, do not resolve the conversations:
it is up to the reviewer who opened the discussion to evaluate the new
iteration and close it. Simply add a comment notifying if the request has been
addressed, or dismissed with an explanation.

It is recommended to double check your Git history before you force-push your
changes and re-request a review to save a few unnecessary iterations and waste
reviewers time.

All PR must stay open at least 24 hours to address the difference in timezones.


[kiss]: https://en.wikipedia.org/wiki/KISS_principle
[git-commit-msg]: https://chris.beams.io/posts/git-commit/
