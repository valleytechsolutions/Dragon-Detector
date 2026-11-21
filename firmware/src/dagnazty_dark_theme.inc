// DagNazty Dark: the supplied skull recolored by tools/render_dagnazty_dark_art.py
// (bone skull, glowing sockets, blood hood) on a black field. Appended IDs keep
// every saved theme and watch selection in place.
static constexpr unsigned DagNaztyDarkTheme=46,DagNaztyDarkWatch=49;
// Digital DagNazty faces, reached by swiping the clock: 50 light, 51 dark.
static constexpr unsigned DagDigitalWatch=50,DagDarkDigitalWatch=51;
// Themes that open those digital faces (theme + 3 = watch, like every theme >= 10).
static constexpr unsigned DagNaztyDigitalTheme=47,DagNaztyDarkDigitalTheme=48;
static bool dagNaztyTheme(unsigned id){return id==23||id==DagNaztyDarkTheme||id==DagNaztyDigitalTheme||id==DagNaztyDarkDigitalTheme;}
static bool dagNaztyDarkTheme(unsigned id){return id==DagNaztyDarkTheme||id==DagNaztyDarkDigitalTheme;}
