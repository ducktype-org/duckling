1. Set your name in `git`:  
    `git config --local user.name = NameSurname`

2. Think on what to change
    * If there isn't an issue/ticket for it, create it

3. Make branch through GH issue page

4. Do some coding
    * Also write some tests and docs!

5. Make pull request

6. Get a review

7. Make corrections and changes

8. Repeat 6-7 until change is accepted
    * and they pass tests!
    
9. Merge change
    * Use `squash and merge` for standard PR
    * see <ForkSyncing.md> for merging between forks
    * Your squashed commit name is: `CommitType: short description (#PR-number / no-ticket)`
    * If multiple commit types fit your commit, pick the best ones, and separate them with coma
      `CommitType1, CommitType2: short description`
    * CommitTypes:
        * `Add`- addition of code, docs, tests, etc
        * `Refactor` - refactor of code
        * `Fix` - fix of a bug/issue
        * `Update` - update of code, or other stuff due to time passing (new version, migration, etc)
        * `Change` - some change of code that doesn't fit three of the above
        * `HotFix` - hot fix of a bug/issue
        * `Maintenance` - repository maintenance -- related to git/GH
        * `Delete`- deletion of some code, or other stuff
    * Other lines of commit description should be used with common sense

