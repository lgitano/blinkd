# This file is from the vbox program and changed for use with the
# blinkd program by W. Martin Borgert <debacle@debian.org>

# First we clear the touchtone sequences and remove all entries from
# the callerid breaklist.

vbox_init_touchtones

vbox_breaklist rem all

# If variable VBOX_FLAG_STANDARD is TRUE we must play the standard
# message...

if { "$vbox_flag_standard" == "TRUE" } {

   set RC [ vbox_put_message $vbox_msg_standard ]

   vbox_pause 500

   if { "$RC" == "HANGUP" } {
      return
   }
}

# If variable VBOX_FLAG_BEEP is TRUE we must play the beep
# message...

if { "$vbox_flag_beep" == "TRUE" } {

   set RC [ vbox_put_message $vbox_msg_beep ]

   vbox_pause 500

   if { "$RC" == "HANGUP" } {
      return
   }
}

# If variable VBOX_FLAG_RECORD is TRUE we must record a new
# message...

if { "$vbox_flag_record" == "TRUE" } {

   set VBOX_NEW_MESSAGE "$vbox_var_spooldir/incoming/$vbox_var_savename"

   set RC [ vbox_get_message $VBOX_NEW_MESSAGE $vbox_var_rectime ]

   vbox_pause 1000

   exec -- $vbox_var_bindir/vboxmail "$VBOX_NEW_MESSAGE" "$vbox_caller_name" "$vbox_caller_id" "$vbox_user_name"

    # The following three lines are one way to activate blinking of the
    # Scroll-Lock LED with blinkd.
    ## set number_msgs [ vbox_get_nr_new_messages $vbox_var_spooldir/incoming ]
    ## exec -- /usr/bin/blink --numlockled --rate=$number_msgs
    # The other way is the easy one:
    exec -- /usr/bin/blink --numlockled --rate=+

   if { "$RC" == "HANGUP" } {
      return
   }

   if { "$RC" == "TIMEOUT" } {

      vbox_put_message $vbox_msg_timeout

      vbox_pause 500
   }
}
