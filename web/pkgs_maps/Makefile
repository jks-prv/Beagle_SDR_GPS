#
# How to update the leaflet-based maps package
#
# DANGER: if aggregating into pkgs_maps/pkgs_maps.css then all the CSS url()s need to be changed
# to point to their original directories.
# e.g. leaflet/leaflet.css:url(images/marker-icon.png) => url(leaflet/images/marker-icon.png)
#
#
# leaflet_js: [     // following template at: https://cloud.maptiler.com/maps/hybrid/leaflet-gl
#
#   // from https://leafletjs.com/download.html
#   'pkgs_maps/leaflet/leaflet.js',
#   'pkgs_maps/leaflet/leaflet.css',    // DANGER: fix as described above
#   // ALSO: copy distro leaflet/images/ subdirectory to pkgs_maps/leaflet/images/
#
#   // from https://api.tiles.mapbox.com/mapbox-gl-js/v0.53.0/mapbox-gl.js
#   'pkgs_maps/leaflet/mapbox-gl/mapbox-gl.53.js',
#   'pkgs_maps/leaflet/mapbox-gl/mapbox-gl.53.css',    // no fixes needed as described above
#   // ALSO: download https://api.tiles.mapbox.com/mapbox-gl-js/v0.53.0/mapbox-gl.js.map to pkgs_maps/leaflet/mapbox-gl/
#   // from https://cdn.klokantech.com/mapbox-gl-leaflet/latest/leaflet-mapbox-gl.js
#   'pkgs_maps/leaflet/mapbox-gl/leaflet-mapbox-gl.js',
#
#   // from https://github.com/Leaflet/Leaflet.markercluster
#   'pkgs_maps/leaflet/MarkerCluster/leaflet.markercluster.js',
#   'pkgs_maps/leaflet/MarkerCluster/MarkerCluster.css',           // no fixes needed as described above
#   'pkgs_maps/leaflet/MarkerCluster/MarkerCluster.Default.css',   // no fixes needed as described above
#
#   // from https://github.com/joergdietrich/Leaflet.Terminator
#   'pkgs_maps/leaflet/js/Terminator.js',
#   
#   // from https://github.com/Leaflet/Leaflet.Graticule
#   'pkgs_maps/leaflet/js/Graticule.js',
#   
#   // from https://github.com/gokertanrisever/leaflet-ruler
#   'pkgs_maps/leaflet/ruler/leaflet-ruler.js',
#   'pkgs_maps/leaflet/ruler/leaflet-ruler.css',    // DANGER: fix as described above
#
#   // from https://github.com/windycom/leaflet-kml
#   'pkgs_maps/leaflet/js/L.KML.js',
#
#   'pkgs_maps/leaflet/js/Zoom_TDoA.js',
# ],
#
#
# CAUTION: .js files below have to be in a very specific order to prevent global var definition errors at load time
#

# files that are already minimized and not to be disturbed
PKGS_MAPS_JS_NOOPT := web/pkgs_maps/leaflet/leaflet.js
PKGS_MAPS_JS_NOOPT += $(wildcard web/pkgs_maps/leaflet/mapbox-gl/mapbox-gl.*.js)
PKGS_MAPS_JS_NOOPT := $(filter-out %.gz,$(filter-out %.min.js,$(PKGS_MAPS_JS_NOOPT)))

PKGS_MAPS_JS := web/pkgs_maps/leaflet/mapbox-gl/leaflet-mapbox-gl.js
PKGS_MAPS_JS += $(wildcard web/pkgs_maps/leaflet/js/*.js)
PKGS_MAPS_JS += $(wildcard web/pkgs_maps/leaflet/MarkerCluster/*.js)
PKGS_MAPS_JS += $(wildcard web/pkgs_maps/leaflet/ruler/*.js)

# fixme xxx jks support for reverting to Google maps in TDoA
FILES_EMBED_JS_NOPKG += $(wildcard web/pkgs_maps/gmaps/*.js)

# NB: %-src.js in the following list
PKGS_MAPS_JS := $(filter-out %.gz,$(filter-out %-src.js,$(filter-out %.min.js,$(PKGS_MAPS_JS))))
FILES_MAPS_JS_PKG = $(PKGS_MAPS_JS) $(PKGS_MAPS_JS_NOOPT)

PKGS_MAPS_CSS := web/pkgs_maps/leaflet/leaflet.css
PKGS_MAPS_CSS += $(wildcard web/pkgs_maps/leaflet/mapbox-gl/mapbox-gl.*.css)
PKGS_MAPS_CSS += $(wildcard web/pkgs_maps/leaflet/MarkerCluster/*.css)
PKGS_MAPS_CSS += $(wildcard web/pkgs_maps/leaflet/ruler/*.css)
PKGS_MAPS_CSS := $(filter-out %.gz,$(filter-out %.min.css,$(PKGS_MAPS_CSS)))
FILES_MAPS_CSS_PKG = $(PKGS_MAPS_CSS)

PKGS_MAPS_PNG := $(wildcard web/pkgs_maps/leaflet/images/*.png)
PKGS_MAPS_PNG += $(wildcard web/pkgs_maps/leaflet/ruler/*.png)
PKGS_MAPS_PNG := $(filter-out %.gz,$(filter-out %.min.png,$(PKGS_MAPS_PNG)))
FILES_ALWAYS += $(subst web/,,$(PKGS_MAPS_PNG))

PKGS_MAPS_ALL := $(PKGS_MAPS_JS) $(PKGS_MAPS_CSS) $(PKGS_MAPS_PNG)
PKGS_MAPS_FILE_JS = web/pkgs_maps/pkgs_maps.min.js
PKGS_MAPS_FILE_CSS = web/pkgs_maps/pkgs_maps.min.css
PKGS_MAPS_EMBED_NW = pkgs_maps/pkgs_maps.min.js.gz pkgs_maps/pkgs_maps.min.css.gz

.PHONY: foptim_files_maps
foptim_files_maps: foptim_maps $(PKGS_MAPS_FILE_JS) $(PKGS_MAPS_FILE_CSS)

.PHONY: foptim_maps
foptim_maps: $(FILE_OPTIM)
	@echo
	@echo '### foptim_maps'
# NB: "-zip" not used here so resulting .min.js files can be packaged
	$(FILE_OPTIM) -t11 $(PKGS_MAPS_JS) $(PKGS_MAPS_CSS)
	$(FILE_OPTIM) -t12 -zip $(PKGS_MAPS_PNG)
	@echo
	@echo FILES_MAPS_JS_PKG = $(FILES_MAPS_JS_PKG)
	@echo FILES_MAPS_CSS_PKG = $(FILES_MAPS_CSS_PKG)

$(PKGS_MAPS_FILE_JS): $(FILES_MAPS_JS_PKG)
	@echo
	cat $(PKGS_MAPS_JS_NOOPT) `$(FILE_OPTIM) -t13 -l -js $(PKGS_MAPS_JS)` >$(PKGS_MAPS_FILE_JS)
# can't do this because it may not be true during EDATA_DEVEL mode
#	@echo 'kiwi_check_js_version.push({ VERSION_MAJ:' $(VERSION_MAJ) ', VERSION_MIN:' $(VERSION_MIN) ', file:"web/pkgs_maps/pkgs_maps.js" });' >>$(PKGS_MAPS_FILE_JS)
	gzip --fast --keep --force $(PKGS_MAPS_FILE_JS)

$(PKGS_MAPS_FILE_CSS): $(FILES_MAPS_CSS_PKG)
	@echo
	cat `$(FILE_OPTIM) -t14 -l -css $(PKGS_MAPS_CSS)` >$(PKGS_MAPS_FILE_CSS)
	gzip --fast --keep --force $(PKGS_MAPS_FILE_CSS)

.PHONY: loptim_maps
loptim_maps: $(FILE_OPTIM)
	-ls -la `$(FILE_OPTIM) -l $(PKGS_MAPS_ALL)` $(PKGS_MAPS_FILE_JS) $(PKGS_MAPS_FILE_CSS)

.PHONY: roptim_maps
roptim_maps: $(FILE_OPTIM)
#	rm -f `$(FILE_OPTIM) -l $(PKGS_MAPS_ALL)` $(PKGS_MAPS_FILE_JS) $(PKGS_MAPS_FILE_CSS)
