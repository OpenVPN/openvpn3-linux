# Thank you for your contribution

For patches to be considered, please ensure you have **read, understood and follows** the requirements of the [Contributor Agreement](https://raw.githubusercontent.com/OpenVPN/openvpn3-linux/master/CLA.md).  This project is licensed under [AGPLv3](https://github.com/OpenVPN/openvpn3-linux/blob/master/COPYRIGHT.md)

Your contribution is to a information security related project.  Therefore we have a few requirements:

  * You must submit all patches with your full name
  * You must submit all patches using an e-mail address you can be reached on
  * Your commit message **must** have a `Signed-off-by:` (added via `git commit -s`)
  * Your final code contribution **must** be sent to the openvpn-devel@lists.sourceforge.net mailing list.

Patches related to documentation only might not be so strictly handled, but anything touching code **must** be follow these points.  Failing to do so will
result in your pull-request being closed without any further considerations.

You are welcome to open a pull-request this way, but they are primarily used for discussion only. Documentation only related pull-requests may be accepted
more easily.  All code patches must eventually go to the openvpn-devel mailing list for final review:

* https://lists.sourceforge.net/lists/listinfo/openvpn-devel

Please send your patch using [git-send-email](https://git-scm.com/docs/git-send-email).  These first few lines here configures a couple of standard values for your convenience:

    $ git config --local sendemail.to openvpn-devel@lists.sourceforge.net
    $ git config --local sendemail.chainreplyto false
    $ git config --local format.subjectPrefix "PATCH openvpn3-linux"

Other settings you might want to look into are [`sendemail.smtpserver`](https://git-scm.com/docs/git-send-email#Documentation/git-send-email.txt---smtp-serverlthostgt) and [`sendemail.from`](https://git-scm.com/docs/git-send-email#Documentation/git-send-email.txt---fromltaddressgt).

To send your last commit to the mailing list, run:

    $ git send-email HEAD~1

See the examples section in [`git format-patch`](https://git-scm.com/docs/git-format-patch#_examples) and the ["SPECIFYING REVISIONS"](https://git-scm.com/docs/gitrevisions#_specifying_revisions) section for further information how to select commit ranges.

