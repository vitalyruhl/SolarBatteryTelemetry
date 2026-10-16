# SIL-Regelungsplanung – Runde 3

Deutsche Diskussionsunterlagen für die gemeinsame Besprechung. Keine implementierte oder freigegebene Automatik. Die Ablage liegt auf Wunsch im SBT-Repository; der Regelungsentwurf betrifft SIL.

## Hauptablauf

[SIL – Ablaufdiagramm für die gemeinsame Diskussion](<SIL - Ablaufdiagramm für die gemeinsame Diskussion.md>)

## Unterprogramme

- [Frische und Plausibilität prüfen](01-messwerte.md)
- [Lüfterbedarf aus Akku- und Umgebungstemperatur bestimmen](02-luefter.md)
- [Richtungsabhängige Schutzprüfung](03-schutzpruefung.md)
- [Laden oder Entladen begrenzen oder sperren](04-schutzreaktion.md)
- [Energie über Abend und Morgen verteilen](05-energieplanung.md)
- [Aktuelles Energiebudget und Leistungsobergrenze berechnen](06-energiebudget.md)
- [Netz-PID berechnet Leistung für den Hausbedarf](07-netz-pid.md)
- [Sollwert senden und tatsächliche Wirkung prüfen](08-senden-ruecklesen.md)
- [Einstellungen beim Start und per Callback übernehmen](09-einstellungen-callback.md)

## Selbst als PDF ausgeben

1. In VS Code die Erweiterung **Markdown Preview Enhanced** installieren: Kennung `shd101wyy.markdown-preview-enhanced`.
2. Eine dieser Markdown-Dateien öffnen und über die Befehlspalette **Markdown Preview Enhanced: Open Preview to the Side** aufrufen.
3. In der Vorschau rechtsklicken und **Chrome (Puppeteer) → PDF** auswählen. Dafür muss Chrome installiert bzw. über `chromePath` erreichbar sein.
4. PDF öffnen und drucken. Die Diagrammdateien enthalten bereits A4-Querformat im Vorspann.

[Erweiterung und Mermaid-Unterstützung](https://github.com/shd101wyy/vscode-markdown-preview-enhanced) · [Offizielle PDF-Exportanleitung](https://github.com/shd101wyy/markdown-preview-enhanced/blob/master/docs/puppeteer.md)

Das automatische Layout kann anders umbrechen als die zuvor von Hand auf drei Seiten gesetzte PDF. Besonders den langen Hauptablauf vor dem Druck auf Schriftgröße und Seitenumbrüche prüfen; die kleinen Unterprogramme sind leichter einzeln zu drucken. Änderungen an Markdown aktualisieren die alte PDF nicht: erneut exportieren. Keine KI und keine Tokens für diesen Export erforderlich.
