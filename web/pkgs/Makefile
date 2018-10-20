FILES_EMBED += pkgs/w3.css
FILES_EMBED += $(subst web/,,$(wildcard web/pkgs/js/*.js))

FILES_EMBED += $(subst web/,,$(wildcard web/pkgs/leaflet/*.js web/pkgs/leaflet/*.css))
FILES_EMBED += $(subst web/,,$(wildcard web/pkgs/leaflet/MarkerCluster/*.js web/pkgs/leaflet/MarkerCluster/*.css))

FILES_EMBED += $(subst web/,,$(wildcard web/pkgs/xdLocalStorage/*.html)) $(subst web/,,$(wildcard web/pkgs/xdLocalStorage/*.js))

########

# FIXME: prevent "out of vm" crashes during compile on Debian 7 by better balancing embed/always sizes
FILES_EMBED += $(subst web/,,$(wildcard web/pkgs/leaflet/images/*.png))
FILES_EMBED += $(subst web/,,$(wildcard web/pkgs/leaflet/mapbox-gl/*.js web/pkgs/leaflet/mapbox-gl/*.map web/pkgs/leaflet/mapbox-gl/*.css))

########

FILES_ALWAYS += $(subst web/,,$(wildcard web/pkgs/font-awesome-4.6.3/css/*))  $(subst web/,,$(wildcard web/pkgs/font-awesome-4.6.3/fonts/*))
FILES_ALWAYS += $(subst web/,,$(wildcard web/pkgs/text-security/text-security-disc.*))
