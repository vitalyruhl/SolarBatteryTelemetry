---
puppeteer:
  landscape: true
  format: "A4"
  printBackground: true
---

# SIL – Sollwert senden und tatsächliche Wirkung prüfen

Stand: 16.10.2026, Runde 3. Grober Diskussionsentwurf, keine Freigabe zur Umsetzung.

**Ein- und Ausgang:** Zulässiger Sollwert bzw. freigegebener MOS-Befehl → bestätigter, ausstehender oder fehlgeschlagener Zustand.

**Formen:** Rechteck = Anweisung; doppelte Seitenlinien = Unterprogramm; Raute = Entscheidung; abgerundet = Start/Ende.

```mermaid
flowchart TD
    A([Anforderung]) --> B[Berechtigung und aktuelle Schutzlage nochmals prüfen]
    B --> C{Anforderung zulässig?}
    C -- Nein --> D[Blockiert melden]
    C -- Ja --> E{Änderung oder notwendige Wiederholung?}
    E -- Nein --> F[Frische Rückmeldung auswerten]
    E -- Ja --> G[Befehl senden<br/>Transportintervalle beachten]
    G --> H[Ausstehend markieren<br/>Regelzyklus nicht blockieren]
    H --> F
    F --> I{Passende neue Rückmeldung vorhanden?}
    I -- Ja --> J[Zustand und Wirkung getrennt bewerten]
    I -- Nein --> K{Frist überschritten?}
    K -- Nein --> L[Ausstehend lassen]
    K -- Ja --> M[Fehler melden<br/>Rückfall und begrenzte Wiederholung qualifizieren]
    D --> N([Ergebnis zurückgeben])
    J --> N
    L --> N
    M --> N
```

## Annahmen und offene Punkte

Eine Transportbestätigung ist keine bestätigte MOS-Stellung. MAX ist nur eine Anforderung, keine garantierte Leistung. Tatsächliche AC-Leistung hängt auch von PV, Batterie und Inverter ab. Schwankende ESP-Rücklesung ist derzeit akzeptiert; automatische MOS-Wiederholungen sind damit noch nicht qualifiziert. Fristen und Wiederholungsgrenzen sind Protokolldetails, nicht neue Benutzereinstellungen.
