---
puppeteer:
  landscape: true
  format: "A4"
  printBackground: true
---

# SIL – Laden oder Entladen begrenzen oder sperren

Stand: 16.10.2026, Runde 3. Grober Diskussionsentwurf, keine Freigabe zur Umsetzung.

**Ein- und Ausgang:** Richtungsbezogene Anforderungen, Freigaben und Messwerte → begrenzter Sollwert, zulässige MOS-Anforderung und Ergebnis.

**Formen:** Rechteck = Anweisung; doppelte Seitenlinien = Unterprogramm; Raute = Entscheidung; abgerundet = Start/Ende.

```mermaid
flowchart TD
    A([Schutzanforderung]) --> B{Harte Sperre erforderlich?}
    B -- Nein --> C{Laden betroffen?}
    C -- Ja --> D[PV-Verbrauch am Inverter gezielt erhöhen<br/>nur innerhalb Freigaben und Grenzen]
    C -- Nein --> E[Entladeleistung reduzieren]
    D --> F[Netto-Akkustrom prüfen<br/>keine zusätzliche Entladung erzwingen]
    E --> G
    F --> G[Explizites Inverter-AUS und Negativpreis-Stopp beachten]
    B -- Ja --> H[Betroffene Stromrichtung sicher sperren lassen]
    H --> I{MOS-Steuerung und Automatik erlaubt<br/>und Strompfad qualifiziert?}
    I -- Ja --> J[Betroffenen MOS-Befehl anfordern]
    I -- Nein --> K[Keine automatische MOS-Schaltung<br/>unerfüllte Schutzanforderung melden]
    J --> L[[Befehl und Wirkung prüfen]]
    K --> M([Schutzstatus zurückgeben])
    G --> L
    L --> M
```

## Annahmen und offene Punkte

Bei gleichzeitigen Anforderungen den strengeren zulässigen Betrieb bestimmen; niemals Laden gegen Entladeschutz ausspielen. Der vorhandene BMS-Eigenschutz muss unabhängig wirksam bleiben. Inverter 0 W verhindert PV-Ladung nicht. Eine fehlende MOS-Berechtigung darf deshalb nicht als erfolgreich ausgeführte Ladesperre gelten. Wie PV bei gesperrter Batterie sicher weiterläuft, ist hardwareseitig offen. Ladegerät-Stopp und weitere Rückfallebenen sind erst zu qualifizieren.
