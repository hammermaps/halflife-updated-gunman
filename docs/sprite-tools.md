# GoldSrc-SPR-Werkzeug

`tools/spr_tool.py` ist eine abhängigkeitsfreie Python-CLI für das klassische
GoldSrc-SPR-Format. Grundlage sind die im eingebetteten SDK dokumentierten
`dsprite_t`-, Frame- und Gruppenstrukturen in
`utils/sprgen/spritegn.h`.

```bash
python3 tools/spr_tool.py unpack input.spr spr-src/mein_sprite
python3 tools/spr_tool.py pack spr-src/mein_sprite out/mein_sprite.spr
./spr-src/decompile_all.sh
./scripts/build-sprites.sh
```

Der Packer schreibt SPR v2 aus 8-Bit-Palette-BMPs; maximal 256x256 Pixel,
jede Kante durch acht teilbar. Er erhält die mit `unpack` erzeugte
`sprite.json` als maßgebliches Manifest und bewahrt damit Header-Metadaten,
Frame-Reihenfolge, Origins sowie Frame-Gruppen und ihre Intervalle. Der
Unpacker verarbeitet SPR v1/v2 einschließlich Frame-Gruppen. V1 enthält keine
Palette; die exportierten BMPs verwenden deshalb Graustufen-Indizes und die
Manifestdatei markiert diese Grenze.

`spr-src/decompile_all.sh` extrahiert den gesamten Retail-Ordner nach
`spr-src/<Dateiname-ohne-Endung>/` und übernimmt die zugehörigen
`.txt`-Definitionen nach `spr-src/text/`. `scripts/build-sprites.sh` packt
alle vorhandenen Manifeste nach `out/sprites/` und kopiert die Textdefinitionen
dorthin. `scripts/stage-mod.sh` ruft diesen Schritt automatisch auf und
überlagert die erzeugten Sprites in jeden Staging-Baum.

GitHub-Recherche: `iQuitt/Vortigaunt` bietet Sprite-Erstellung, ist aber ein
Windows-Desktopwerkzeug; `DrAbcOfficial/SPRView.Net` ist plattformübergreifend,
aber ein Viewer. Beide sind daher Referenzen, nicht Build-Abhängigkeiten. Das
lokale Werkzeug bleibt auf Linux und Windows mit Python 3 verwendbar.
