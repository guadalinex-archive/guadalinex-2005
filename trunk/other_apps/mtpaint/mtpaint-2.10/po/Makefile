include ../_conf.txt

langs = es cs fr pt pt_BR

all:
	for iii in $(langs); do msgfmt -o $$iii.mo $$iii.po; done

install:
	for iii in $(langs); do mkdir -p $(MT_LANG_DEST)/$$iii/LC_MESSAGES/; done
	for iii in $(langs); do cp $$iii.mo $(MT_LANG_DEST)/$$iii/LC_MESSAGES/mtpaint.mo; done

clean:
	rm *.mo
