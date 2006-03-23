<CHRP-BOOT>
<COMPATIBLE>
MacRISC MacRISC3 MacRISC4
</COMPATIBLE>
<DESCRIPTION>
${DISTRIBUTION_NAME} PowerPC first stage CDROM bootloader
</DESCRIPTION>
<BOOT-SCRIPT>
" screen" output
load-base release-load-area
boot cd:,\install\yaboot
</BOOT-SCRIPT>
</CHRP-BOOT>
