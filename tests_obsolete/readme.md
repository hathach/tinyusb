# Test-Driven Development

[Test-Driven Development (TDD)](http://en.wikipedia.org/wiki/Test-driven_development) is a development process that relies on the repetition of a very short development cycle: first the developer writes an (initially failing) automated test case that defines a desired improvement or new function, then produces the minimum amount of code to pass that test, and finally refactors the new code to acceptable standards.

In this project, TDD is performed by the help of Ceedling, Unity & CMock as a testing framework. However, due to my limited time, not all the code base is tested yet, and it will be indeed an challenging to keep the test up to the code. 

More detail on TDD can be found at

- [James W. Grenning's book "Test Driven Development for Embedded C"](http://www.amazon.com/Driven-Development-Embedded-Pragmatic-Programmers/dp/193435662X)
- [throwtheswitch's Ceedling, CMock & Unity](http://throwtheswitch.org/)

## Continuous Integration

Continuous Integration (CI) is used to automatically run all the tests whenever there is a change in the code base. This makes sure that a modification of a file won't break any tests or functionality of others, verifying they all passed. 

As many other open source project, tinyusb uses Travis-CI server (free for OOS). You can find my project on Travis here https://travis-ci.org/hathach/tinyusb  

![Build Status](https://travis-ci.org/hathach/tinyusb.svg?branch=master)