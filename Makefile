.PHONY: all app-only clean
all:
	$(MAKE) -C plugin
	$(MAKE) -C app
	mkdir -p dist
	cp plugin/discordnx2.nso dist/discordnx2.nso
	cp app/DiscordNX2.nro dist/DiscordNX2.nro
app-only:
	$(MAKE) -C app
	mkdir -p dist
	cp app/DiscordNX2.nro dist/DiscordNX2.nro
clean:
	$(MAKE) -C plugin clean
	$(MAKE) -C app clean
	rm -rf dist
