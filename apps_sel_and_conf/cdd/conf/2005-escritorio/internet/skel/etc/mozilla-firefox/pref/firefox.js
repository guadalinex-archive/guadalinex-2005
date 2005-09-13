// This is the Debian specific preferences file for Mozilla Firefox
// You can make any change in here, it is the purpose of this file.
// You can, with this file and all files present in the
// /etc/mozilla-firefox/pref directory, override any preference that is
// present in /usr/lib/mozilla-firefox/defaults/pref directory.
// While your changes will be kept on upgrade if you modify files in
// /etc/mozilla-firefox/pref, please note that they won't be kept if you
// do them in /usr/lib/mozilla-firefox/defaults/pref.

pref("extensions.update.enabled", true);
pref("extensions.update.autoUpdateEnabled", false);
pref("extensions.update.autoUpdate", false);

// Use LANG environment variable to choose locale
pref("intl.locale.matchOS", true);

// Disable default browser checking.
pref("browser.shell.checkDefaultBrowser", false);

// Handle ed2k links
pref("network.protocol-handler.app.ed2k", "/usr/bin/ed2k");
pref("network.protocol-handler.external.ed2k", true);
pref("network.protocol-handler.warn-external.ed2k", false);

// Wipe out annoying security warnings
pref("security.warn_entering_secure", false);
pref("security.warn_leaving_secure", false);
pref("security.warn_submit_insecure", false);
