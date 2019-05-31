FILES_EMBED_JS += $(wildcard web/pkgs/js/*.js)
# specify explicitly so as not to include unwanted .js files
FILES_EMBED_JS += web/pkgs/xdLocalStorage/xdLocalStorage.js web/pkgs/xdLocalStorage/xd-utils.js

FILES_EMBED_CSS += web/pkgs/w3.css

# NB: FILES_EMBED_MISC not FILES_EMBED_HTML because want to leave intact
FILES_EMBED_MISC += $(wildcard web/pkgs/xdLocalStorage/*.html)

########

FILES_ALWAYS += $(subst web/,,$(wildcard web/pkgs/font-awesome/css/*))
FILES_ALWAYS += $(subst web/,,$(wildcard web/pkgs/font-awesome/fonts/*))
FILES_ALWAYS += $(subst web/,,$(wildcard web/pkgs/text-security/text-security-disc.*))
