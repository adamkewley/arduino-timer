# arduino-timer

A library-free implementation of "Elegoo The Most Complete Starter Kit"'s timer creation guide.

The original guide can be downloaded [here](https://www.elegoo.com/tutorial/Elegoo%20The%20Most%20Complete%20Starter%20Kit%20for%20UNO%20V2.0.2020.4.3.zip). The timer tutorial is in `English/Part 3 Multi-module Combination Course/3.9    Creating Timer`.

I liked the idea of making a timer with an alarm, so I went through Elegoo's guide for making one. I think the tutorial is good--Elegoo's tutorials are, in general, good--but, as a learning exercise, I modified the tutorial's implementation. Main changes:

- This implementation is in a single source file, making it easier to browse
- It uses no libraries. This makes it hardware-dependent, which is "bad", but means that the implementation shows how to *actually* install timer interrupts etc.)
- Top-level components (shift register, display, debouncer, etc.) are written in an object-oriented C++ style that readers might find to be an interesting approach to some of the design problems
