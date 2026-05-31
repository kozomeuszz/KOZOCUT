# KOZOcut

Prosty firmware startowy dla przecinarki kabli na `ESP32-C3 Mini` i dwóch sterownikach `TMC2209`.

## Założenia pinów

- `Extruder DIR`: `GPIO0`
- `Extruder STEP`: `GPIO1`
- `Guillotine DIR`: `GPIO7`
- `Guillotine STEP`: `GPIO3`
- `Enable obu driverów`: `GPIO4`

## Ważne połączenia

- `VM` driverów `TMC2209` -> `24V`
- `VIO` driverów `TMC2209` -> `3.3V` z `ESP32-C3`
- `GND` zasilacza, `GND` driverów i `GND` `ESP32-C3` muszą być wspólne
- `STEP`, `DIR`, `EN` z `ESP32-C3` idą do odpowiednich wejść obu driverów

## Co robi firmware

- stawia Access Point `KOZOcut`
- sieć jest otwarta, bez hasła
- panel jest pod `http://192.168.4.1`
- pozwala ustawić:
  - ilość sztuk
  - długość kabla w `mm`
  - kalibrację `steps/mm`
  - szybkość podawania i cięcia
  - liczbę kroków gilotyny na jedno cięcie
- pozwala wykonać:
  - start produkcji
  - stop
  - ręczny posuw `jog`
  - test pojedynczego cięcia

## Kalibracja `steps/mm`

Wartość początkowa w kodzie to `52.4590 steps/mm`.
Ta wartość została przeliczona z pomiaru, w którym zadane `30 mm` dawało realnie `25 mm`.

Praktyczna kalibracja:

1. Ustaw w panelu znaną wartość, np. przesuw `100 mm`.
2. Zmierz realny wysuw kabla.
3. Przelicz:

```text
nowe_steps_per_mm = stare_steps_per_mm * (zadana_dlugosc_mm / zmierzona_dlugosc_mm)
```

Przykład:

```text
52.4590 * (100 / 96.5) = 54.36
```

## Budowanie i wgrywanie

```bash
pio run
pio run -t upload
pio device monitor
```

## Uwaga mechaniczna

Kod zakłada, że jedno cięcie to zawsze stała liczba kroków `guillotineStepsPerCut`.
Domyślna wartość pełnego obrotu gilotyny jest ustawiona na `1600`.
Jeśli w praktyce pozycja noża potrafi się rozjechać, dodaj krańcówkę referencyjną dla gilotyny.
