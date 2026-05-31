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
