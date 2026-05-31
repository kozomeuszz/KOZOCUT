# KOZOcut

KOZOcut to firmware dla kompaktowej przecinarki do kabli opartej o `ESP32-C3 Super Mini`, dwa silniki krokowe `NEMA17` i sterowniki `TMC2209`. Urządzenie uruchamia własny punkt Wi-Fi `KOZOcut`, a panel sterowania jest dostępny pod adresem `http://192.168.4.1`.

Projekt jest przygotowany tak, aby obudowę i elementy mechaniczne można było wydrukować w częściach na drukarce 3D, a elektronikę złożyć z łatwo dostępnych modułów.

## Funkcje firmware

- podawanie kabla na zadaną długość w `mm`
- produkcja zadanej liczby odcinków
- pojedyncze cięcie testowe
- ręczny posuw `jog`
- regulacja `steps/mm`
- regulacja prędkości podawania i cięcia
- ustawianie liczby kroków gilotyny na jedno cięcie
- zapisywanie konfiguracji w pamięci `ESP32-C3`

## Części

| Element | Ilość | Uwagi |
| --- | ---: | --- |
| `ESP32-C3 Super Mini` USB-C | 1 | Główny kontroler Wi-Fi i logika sterowania |
| Sterownik silnika krokowego `TMC2209 V2.0` z radiatorem | 2 | Jeden dla podajnika, jeden dla gilotyny |
| Silnik krokowy `NEMA17 17HS4401`, 1.8 deg, 1.5 A, 42 N.cm | 2 | Podajnik i napęd gilotyny |
| Zasilacz impulsowy `12 V` lub `24 V` | 1 | Zalecane `24 V` dla lepszej dynamiki silników |
| Przetwornica buck `MP1584EN` | 1 | Obniża napięcie zasilacza do `5 V` dla ESP32 |
| Płytka rozszerzeń / shield pod `A4988/DRV8825` | 1 | Może służyć jako baza montażowa dla sterowników w formacie Pololu |
| Sprężyna ekstrudera `1.2 x 7.5 x 20 mm` | wg mechaniki | Docisk rolki podającej |
| Łożyska `13 mm` | wg mechaniki | Użyte jako rolki dociskowe ekstrudera |
| Ostrze z regulowanego narzędzia do ściągania izolacji UTP/STP | 1 | Użyte jako element tnący gilotyny i strippera |
| Śruby krzyżakowe z łbem stożkowym `M3` i `M4` | wg potrzeb | Montaż elementów drukowanych i elektroniki |
| Nitonakrętki aluminiowe `M3` i `M4` z łbem płaskim | wg potrzeb | Gwintowane punkty montażowe w częściach mechanicznych |
| Wydrukowane części 3D | wg projektu | Rama, prowadnice, mocowania silników, docisk, osłony |
| Przewody, złącza śrubowe, tulejki | wg potrzeb | Dobierz do finalnej konstrukcji |

Przykładowe źródła części z AliExpress:

- Płytka rozszerzeń sterownika silnika krokowego `DRV8825/A4988` dla Arduino UNO/RAMPS: https://a.aliexpress.com/_EJtjCLG
- Sprężyny ekstrudera `1.2 x 7.5 x 20 mm`: https://a.aliexpress.com/_EzRrIdo
- Silnik krokowy `Usongshine NEMA17 17HS4401`: https://a.aliexpress.com/_EuOhsWE
- `ESP32-C3 Super Mini`: https://a.aliexpress.com/_EHPe6JM
- Zasilacz impulsowy `12 V / 24 V`: https://a.aliexpress.com/_EuOHUfk
- Przetwornica buck `MP1584EN`: https://a.aliexpress.com/_Ez9iknU
- Sterownik `TMC2209 V2.0`: https://a.aliexpress.com/_EGThV0w
- Śruby krzyżakowe z łbem stożkowym `M3/M4`: https://a.aliexpress.com/_Ez9eS9Y
- Nitonakrętki z łbem płaskim `M3/M4`: https://a.aliexpress.com/_EJ4cOce
- Regulowane narzędzie UTP/STP do ściągania izolacji i cięcia kabli, użyte jako dawca ostrza: https://a.aliexpress.com/_EG4WMGQ

## Druk 3D i montaż mechaniczny

Części mechaniczne można drukować osobno, co ułatwia serwis i późniejsze poprawki. Praktyczny podział:

- rama główna
- uchwyt silnika podajnika
- uchwyt silnika gilotyny
- prowadnica kabla
- docisk ekstrudera ze sprężyną i łożyskami `13 mm`
- mocowanie elektroniki
- osłona noża i elementów ruchomych

Po wydruku sprawdź osiowość prowadnicy kabla, docisk łożysk ekstrudera i swobodny ruch gilotyny. Mechanika musi poruszać się lekko przed podłączeniem silników, inaczej sterownik może gubić kroki.

Do montażu drukowanych części użyto śrub `M3` i `M4` z łbem stożkowym oraz nitonakrętek `M3` i `M4`. Nitonakrętki warto osadzać w miejscach serwisowych, gdzie elementy będą wielokrotnie odkręcane, np. przy mocowaniu osłon, uchwytów silników i panelu elektroniki.

Ostrze gilotyny i strippera zostało pozyskane z regulowanego narzędzia do ściągania izolacji i cięcia kabli `UTP/STP`. Przy projektowaniu mocowania ostrza zachowaj sztywne podparcie, możliwość regulacji oraz osłonę strefy cięcia.

## Pinout ESP32-C3

Firmware używa poniższych pinów:

| Funkcja | Pin ESP32-C3 | Sygnał na sterowniku |
| --- | --- | --- |
| Kierunek silnika podajnika | `GPIO0` | `DIR` sterownika podajnika |
| Krok silnika podajnika | `GPIO1` | `STEP` sterownika podajnika |
| Kierunek silnika gilotyny | `GPIO7` | `DIR` sterownika gilotyny |
| Krok silnika gilotyny | `GPIO3` | `STEP` sterownika gilotyny |
| Wspólne włączenie sterowników | `GPIO4` | `EN` / `ENABLE` obu sterowników |
| Dioda statusu na płytce | `GPIO8` | LED |
| Przycisk BOOT | `GPIO9` | wejście z `INPUT_PULLUP` |

`EN` w typowych sterownikach `TMC2209/A4988/DRV8825` jest aktywne stanem niskim. W firmware `GPIO4 = LOW` włącza sterowniki, a `GPIO4 = HIGH` je wyłącza.

## Schemat połączeń

```text
                 +----------------------+
 AC 230 V  ----> | Zasilacz 12/24 V DC  |
                 +----------+-----------+
                            |
                            | +12/24 V
                            v
        +-------------------+-------------------+
        |                                       |
        v                                       v
+---------------+                       +----------------+
| TMC2209 FEED  |                       | TMC2209 CUT    |
| VM  <- +12/24 |                       | VM  <- +12/24  |
| GND <- GND    |                       | GND <- GND     |
| VIO <- 3.3 V  |                       | VIO <- 3.3 V   |
| DIR <- GPIO0  |                       | DIR <- GPIO7   |
| STEP<- GPIO1  |                       | STEP<- GPIO3   |
| EN  <- GPIO4  |                       | EN  <- GPIO4   |
| A/B -> NEMA17 |                       | A/B -> NEMA17  |
+-------+-------+                       +--------+-------+
        |                                        |
        v                                        v
  Silnik podajnika                         Silnik gilotyny

                 +----------------------+
                 | Buck MP1584EN       |
 +12/24 V ------>| IN+              OUT+|---- 5 V ----+
 GND ----------->| IN-              OUT-|---- GND ----+
                 +----------------------+             |
                                                       v
                                             +----------------+
                                             | ESP32-C3 Mini  |
                                             | 5V/VBUS <- 5 V |
                                             | GND     <- GND |
                                             | 3V3     -> VIO |
                                             +----------------+
```

Wszystkie masy muszą być wspólne:

- `GND` zasilacza
- `GND` przetwornicy buck
- `GND` ESP32-C3
- `GND` obu sterowników silników

## Podłączenie sterowników

Minimalne połączenia dla każdego `TMC2209`:

| Pin sterownika | Podajnik | Gilotyna |
| --- | --- | --- |
| `VM` / `MOT+` | `+12/24 V` | `+12/24 V` |
| `GND` zasilania silnika | masa zasilacza | masa zasilacza |
| `VIO` / `VDD` | `3.3 V` z ESP32 | `3.3 V` z ESP32 |
| `GND` logiki | `GND` ESP32 | `GND` ESP32 |
| `STEP` | `GPIO1` | `GPIO3` |
| `DIR` | `GPIO0` | `GPIO7` |
| `EN` | `GPIO4` | `GPIO4` |
| wyjścia silnika | cewki silnika podajnika | cewki silnika gilotyny |

Jeżeli używasz płytki rozszerzeń pod `A4988/DRV8825`, potraktuj ją jako płytkę nośną. Sprawdź opis pinów konkretnego modułu, bo kolejność `STEP`, `DIR`, `EN`, `VMOT`, `VDD` i `GND` może zależeć od wersji shielda.

## Zasilanie

Zalecany układ:

- zasilacz `24 V DC` zasila piny `VM` sterowników silników
- przetwornica `MP1584EN` obniża `24 V` do stabilnych `5 V`
- `5 V` zasila pin `5V/VBUS` płytki `ESP32-C3`
- pin `3V3` z ESP32 zasila logikę `VIO/VDD` sterowników

Nie podawaj `24 V` na ESP32. Przed podłączeniem ESP32 ustaw wyjście przetwornicy buck miernikiem na `5.0 V`.

Praca z wejściem `AC 230 V` zasilacza jest niebezpieczna. Podłączaj stronę sieciową tylko przy odłączonym zasilaniu i zabezpiecz zaciski przed dotykiem.

## Kalibracja

Wartość początkowa w firmware to:

```text
44.0 steps/mm
```

Praktyczna procedura:

1. W panelu ustaw testowy przesuw, np. `100 mm`.
2. Zmierz rzeczywisty wysuw kabla.
3. Przelicz nową wartość:

```text
nowe_steps_per_mm = stare_steps_per_mm * (zadana_dlugosc_mm / zmierzona_dlugosc_mm)
```

Przykład:

```text
44.0 * (100 / 96.5) = 45.60
```

## Budowanie i wgrywanie

Projekt używa PlatformIO.

```bash
pio run
pio run -t upload
pio device monitor
```

Domyślny port w `platformio.ini`:

```ini
monitor_port = /dev/cu.usbmodem1101
upload_port = /dev/cu.usbmodem1101
```

Jeżeli Twoja płytka pojawia się pod innym portem, zmień te wartości albo usuń je i pozwól PlatformIO wykryć port automatycznie.

## Uruchomienie

1. Sprawdź wszystkie połączenia bez podłączonych silników.
2. Ustaw przetwornicę buck na `5 V`.
3. Podłącz ESP32 i wgraj firmware.
4. Podłącz sterowniki i silniki.
5. Włącz zasilanie silników.
6. Połącz się z Wi-Fi `KOZOcut`.
7. Otwórz `http://192.168.4.1`.
8. Wykonaj test `jog` i pojedyncze cięcie.
9. Skalibruj `steps/mm` oraz liczbę kroków gilotyny.

## Uwagi mechaniczne

Kod zakłada, że jedno cięcie to zawsze stała liczba kroków `guillotineStepsPerCut`. Domyślna wartość to `1600`.

Jeżeli pozycja noża potrafi się rozjechać, dodaj krańcówkę referencyjną dla gilotyny albo mechaniczny punkt bazowania. Przy pracy z nożem stosuj osłonę i nie uruchamiaj mechanizmu z odsłoniętą strefą cięcia.

---

# KOZOcut - English Version

KOZOcut is firmware for a compact cable cutting machine based on an `ESP32-C3 Super Mini`, two `NEMA17` stepper motors, and `TMC2209` stepper drivers. The device starts its own `KOZOcut` Wi-Fi access point, and the control panel is available at `http://192.168.4.1`.

The project is designed so the enclosure and mechanical parts can be 3D printed in separate pieces, while the electronics can be assembled from commonly available modules.

## Firmware Features

- cable feeding to a target length in `mm`
- production of a selected number of pieces
- single test cut
- manual `jog` feed
- `steps/mm` calibration
- feed and cutting speed adjustment
- configurable number of guillotine steps per cut
- configuration saved in `ESP32-C3` memory

## Parts

| Part | Quantity | Notes |
| --- | ---: | --- |
| `ESP32-C3 Super Mini` USB-C | 1 | Main Wi-Fi controller and control logic |
| `TMC2209 V2.0` stepper driver with heatsink | 2 | One for the feeder, one for the guillotine |
| `NEMA17 17HS4401` stepper motor, 1.8 deg, 1.5 A, 42 N.cm | 2 | Feeder and guillotine drive |
| `12 V` or `24 V` switching power supply | 1 | `24 V` is recommended for better motor dynamics |
| `MP1584EN` buck converter | 1 | Steps the supply voltage down to `5 V` for the ESP32 |
| `A4988/DRV8825` expansion board / shield | 1 | Can be used as a carrier board for Pololu-style drivers |
| Extruder spring `1.2 x 7.5 x 20 mm` | as needed | Feeder pressure mechanism |
| `13 mm` bearings | as needed | Used as extruder pressure rollers |
| Blade from an adjustable UTP/STP cable stripping tool | 1 | Used as the guillotine and stripper cutting element |
| `M3` and `M4` Phillips countersunk screws | as needed | Mounting printed parts and electronics |
| `M3` and `M4` flat-head aluminum rivet nuts | as needed | Threaded mounting points in mechanical parts |
| 3D printed parts | project-specific | Frame, guides, motor mounts, pressure mechanism, covers |
| Wires, screw terminals, ferrules | as needed | Select according to the final build |

Example AliExpress part sources:

- `DRV8825/A4988` stepper driver expansion board for Arduino UNO/RAMPS: https://a.aliexpress.com/_EJtjCLG
- Extruder springs `1.2 x 7.5 x 20 mm`: https://a.aliexpress.com/_EzRrIdo
- `Usongshine NEMA17 17HS4401` stepper motor: https://a.aliexpress.com/_EuOhsWE
- `ESP32-C3 Super Mini`: https://a.aliexpress.com/_EHPe6JM
- `12 V / 24 V` switching power supply: https://a.aliexpress.com/_EuOHUfk
- `MP1584EN` buck converter: https://a.aliexpress.com/_Ez9iknU
- `TMC2209 V2.0` driver: https://a.aliexpress.com/_EGThV0w
- `M3/M4` Phillips countersunk screws: https://a.aliexpress.com/_Ez9eS9Y
- `M3/M4` flat-head rivet nuts: https://a.aliexpress.com/_EJ4cOce
- Adjustable UTP/STP cable stripping and cutting tool, used as a blade donor: https://a.aliexpress.com/_EG4WMGQ

## 3D Printing and Mechanical Assembly

The mechanical parts can be printed separately, which makes servicing and future revisions easier. A practical split is:

- main frame
- feeder motor mount
- guillotine motor mount
- cable guide
- extruder pressure mechanism with spring and `13 mm` bearings
- electronics mount
- blade and moving-part cover

After printing, check cable guide alignment, extruder bearing pressure, and free guillotine movement. The mechanism must move smoothly before the motors are connected, otherwise the drivers may skip steps.

The printed parts use `M3` and `M4` countersunk screws and `M3`/`M4` rivet nuts. Rivet nuts are useful in service points where parts may be removed repeatedly, such as covers, motor mounts, and the electronics panel.

The guillotine and stripper blade was taken from an adjustable `UTP/STP` cable stripping and cutting tool. When designing the blade mount, keep the blade rigidly supported, adjustable, and covered by a guard around the cutting area.

## ESP32-C3 Pinout

The firmware uses the following pins:

| Function | ESP32-C3 Pin | Driver Signal |
| --- | --- | --- |
| Feeder motor direction | `GPIO0` | Feeder driver `DIR` |
| Feeder motor step | `GPIO1` | Feeder driver `STEP` |
| Guillotine motor direction | `GPIO7` | Guillotine driver `DIR` |
| Guillotine motor step | `GPIO3` | Guillotine driver `STEP` |
| Shared driver enable | `GPIO4` | `EN` / `ENABLE` on both drivers |
| On-board status LED | `GPIO8` | LED |
| BOOT button | `GPIO9` | `INPUT_PULLUP` input |

`EN` on typical `TMC2209/A4988/DRV8825` drivers is active low. In the firmware, `GPIO4 = LOW` enables the drivers and `GPIO4 = HIGH` disables them.

## Wiring Diagram

```text
                 +----------------------+
 AC 230 V  ----> | 12/24 V DC PSU       |
                 +----------+-----------+
                            |
                            | +12/24 V
                            v
        +-------------------+-------------------+
        |                                       |
        v                                       v
+---------------+                       +----------------+
| TMC2209 FEED  |                       | TMC2209 CUT    |
| VM  <- +12/24 |                       | VM  <- +12/24  |
| GND <- GND    |                       | GND <- GND     |
| VIO <- 3.3 V  |                       | VIO <- 3.3 V   |
| DIR <- GPIO0  |                       | DIR <- GPIO7   |
| STEP<- GPIO1  |                       | STEP<- GPIO3   |
| EN  <- GPIO4  |                       | EN  <- GPIO4   |
| A/B -> NEMA17 |                       | A/B -> NEMA17  |
+-------+-------+                       +--------+-------+
        |                                        |
        v                                        v
   Feeder motor                            Guillotine motor

                 +----------------------+
                 | Buck MP1584EN       |
 +12/24 V ------>| IN+              OUT+|---- 5 V ----+
 GND ----------->| IN-              OUT-|---- GND ----+
                 +----------------------+             |
                                                       v
                                             +----------------+
                                             | ESP32-C3 Mini  |
                                             | 5V/VBUS <- 5 V |
                                             | GND     <- GND |
                                             | 3V3     -> VIO |
                                             +----------------+
```

All grounds must be common:

- power supply `GND`
- buck converter `GND`
- ESP32-C3 `GND`
- both stepper driver `GND` pins

## Stepper Driver Wiring

Minimum wiring for each `TMC2209`:

| Driver Pin | Feeder | Guillotine |
| --- | --- | --- |
| `VM` / `MOT+` | `+12/24 V` | `+12/24 V` |
| Motor power `GND` | power supply ground | power supply ground |
| `VIO` / `VDD` | `3.3 V` from ESP32 | `3.3 V` from ESP32 |
| Logic `GND` | ESP32 `GND` | ESP32 `GND` |
| `STEP` | `GPIO1` | `GPIO3` |
| `DIR` | `GPIO0` | `GPIO7` |
| `EN` | `GPIO4` | `GPIO4` |
| motor outputs | feeder motor coils | guillotine motor coils |

If you use an `A4988/DRV8825` expansion board, treat it as a carrier board. Check the pin labels on your specific module, because the order of `STEP`, `DIR`, `EN`, `VMOT`, `VDD`, and `GND` may differ between shield versions.

## Power

Recommended setup:

- `24 V DC` power supply feeds the motor driver `VM` pins
- `MP1584EN` buck converter steps `24 V` down to stable `5 V`
- `5 V` powers the `5V/VBUS` pin of the `ESP32-C3`
- the ESP32 `3V3` pin powers the `VIO/VDD` logic side of the drivers

Do not feed `24 V` into the ESP32. Before connecting the ESP32, set the buck converter output to `5.0 V` with a multimeter.

Working with the `AC 230 V` input side of the power supply is dangerous. Wire the mains side only with power disconnected and protect the terminals from accidental touch.

## Calibration

The firmware default value is:

```text
44.0 steps/mm
```

Practical procedure:

1. In the control panel, set a known test feed, for example `100 mm`.
2. Measure the actual cable feed length.
3. Calculate the new value:

```text
new_steps_per_mm = old_steps_per_mm * (target_length_mm / measured_length_mm)
```

Example:

```text
44.0 * (100 / 96.5) = 45.60
```

## Build and Upload

The project uses PlatformIO.

```bash
pio run
pio run -t upload
pio device monitor
```

Default port in `platformio.ini`:

```ini
monitor_port = /dev/cu.usbmodem1101
upload_port = /dev/cu.usbmodem1101
```

If your board appears on a different port, change these values or remove them and let PlatformIO detect the port automatically.

## First Start

1. Check all wiring with the motors disconnected.
2. Set the buck converter to `5 V`.
3. Connect the ESP32 and upload the firmware.
4. Connect the drivers and motors.
5. Turn on motor power.
6. Connect to the `KOZOcut` Wi-Fi network.
7. Open `http://192.168.4.1`.
8. Run a `jog` test and a single test cut.
9. Calibrate `steps/mm` and the number of guillotine steps.

## Mechanical Notes

The code assumes that one cut is always a fixed number of `guillotineStepsPerCut` steps. The default value is `1600`.

If the blade position drifts over time, add a guillotine homing limit switch or a mechanical reference point. Use a blade cover and do not run the mechanism with the cutting area exposed.
