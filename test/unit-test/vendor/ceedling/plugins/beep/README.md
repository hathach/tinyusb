ceedling-beep
=============

This is a simple plugin that just beeps at the end of a build and/or test sequence. Are you getting too distracted surfing
the internet, chatting with coworkers, or swordfighting while it's building or testing? The friendly beep will let you know
it's time to pay attention again.

This plugin has very few configuration options. At this time it can beep on completion of a task and/or on an error condition.
For each of these, you can configure the method that it should beep.

```
:tools:
  :beep_on_done: :bell
  :beep_on_error: :bell
```

Each of these have the following options:

  - :bell - this option uses the ASCII bell character out stdout
  - :speaker_test - this uses the linux speaker-test command if installed

Very likely, we'll be adding to this list if people find this to be useful.
