#!/usr/bin/python
# -*- coding: utf-8 -*-

import unittest

from notify import NotifyTest


if __name__ == "__main__":
    suite = unittest.TestSuite()
    suite.addTest(NotifyTest)

    runner = unittest.TextTestRunner()

    unittest.main(testRunner = runner)


