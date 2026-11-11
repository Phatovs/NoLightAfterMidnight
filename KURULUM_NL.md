# No Light After Midnight: Mekanik Sistemleri Kurulumu

## 1. Kod (Claude projeye yerleştirdi)
- Kodlar `Source/NoLightAfterMidnight/Mechanics/` içinde. `Build.cs` dosyasına `SlateCore`, `NavigationSystem` ve `Mechanics` include yolu eklendi.
- `.uproject` dosyasına sağ tıkla, **Generate Visual Studio project files** seç, sonra derle.
5. Project Settings > Input > **Default Input Component Class** = `EnhancedInputComponent` olmalı (UE5'te varsayılan zaten bu).

## 2. Sesleri içe aktar
Sesler `Content/Audio/NL/` klasöründe. Editör içe aktarmayı sorarsa kabul et. Sormazsa Content Browser'da o klasöre sağ tıklayıp Import ile ekle.
Adı `_loop` ile biten dosyaları açıp **Looping** seçeneğini işaretle (generator_hum_loop, monitor_hum_loop, drowned_breath_loop, runner_chase_loop, ambient_wind_sea_loop).

| Ses | Nereye atanır |
|---|---|
| generator_hum_loop / start / stop / breakdown | NLGenerator → Hum Loop / Engine Start / Engine Stop / Breakdown |
| repair_hit / repair_mistake / repair_complete / refuel | NLGenerator → Repair Hit / Mistake / Complete / Refuel |
| fuse_switch_on / fuse_switch_off | NLFuseBox → Switch On / Off (Trip için de off) |
| camera_switch / computer_on / computer_off / monitor_hum_loop | NLSecurityDesk |
| light_flash / flash_fail | NLSecurityCamera → Flash / Flash Fail |
| drowned_appear / drowned_breath_loop / drowned_screech / jumpscare | NLDrowned → Appear / Breath Loop / Screech / Jumpscare |
| runner_scream / runner_chase_loop / kill | NLRunner → Chase Start Scream / Chase Loop / Kill |
| footstep | NLPlayerCharacter → Footstep |
| flashlight_click | Player'daki Flashlight bileşeni → Toggle / Empty |
| fuel_pickup | NLFuelCan → Pickup |
| charger_dock / charge_full | NLFlashlightCharger → Dock, Undock / Full |
| day_start / day_end / game_over / win / ambient_wind_sea_loop | NLGameMode |

> Bu sesler kodla sentezlenmiş geçici (placeholder) seslerdir. Final için gerçek kayıtlarla değiştirin; kodda bir değişiklik gerekmez.

## 3. Sahneyi kur
> **Otomatik kurulum yapıldı.** Sesler artık koddan otomatik atanıyor (alan boşsa `Content/Audio/NL/` içinden yüklenir), döngü seslerinin Looping ayarı açık. Lvl_NLTest'e eksik aktörler `Scripts/nl_setup_scene.py` ile kondu: 8 kamera (yer adı, açı ve Drowned noktasıyla birlikte), fener şarjı, benzin bidonu, Runner, NavMesh, masa ve monitör (M_NLCameraMonitor). Haritayı değiştirdikten sonra tekrar çalıştırmak güvenlidir, var olanları çoğaltmaz. Editörde **Tools > Execute Python Script** ile çalıştırabilirsin.
>
> Aşağıdaki adımlar elle kurulum ya da başka harita içindir.

1. **World Settings > GameMode Override** = `NLGameMode` (Default Pawn otomatik olarak `NLPlayerCharacter` olur).
2. Sahneye şunları yerleştir:
   - `NLGenerator` (benzin göstergeli jeneratör)
   - `NLFuseBox` ve hemen yanına `NLFlashlightCharger`
   - `NLFuelCan` (kulübenin içine)
   - 8 tane `NLSecurityCamera`. Her birinin **Camera Name** değerini Cam01…Cam08 yap. Kırmızı oku (DrownedSpawnPoint) Drowned'ın duracağı yere, spot ışığın menziline taşı.
   - `NLSecurityDesk`. **Cameras** dizisine 8 kamerayı sırayla ekle, **Feed Render Target** alanına bir Render Target ata (ör. RT_CameraFeed, 1280x720).
   - `NLDrowned` (Skeletal Mesh ve Anim Class'ı Details'tan ata) ve `NLDrownedDirector`. Director'da **Desk** ve **Placed Drowned** alanlarını seç.
   - `NLRunner`: konduğu yer, kovalamaya başladığı yer olur. Mesh ve Anim Class'ını ata.
   - **NavMeshBoundsVolume**: Runner'ın koşacağı tüm alanı kaplamalı (P tuşuyla yeşil alan görünür).
3. **Monitör materyali:** `FeedTexture` adlı bir Texture parametresi ve `Power` adlı bir Scalar parametresi olsun. Emissive = FeedTexture × Power. Bu materyali NLSecurityDesk'in MonitorMesh'ine ata.

## 4. Animasyonlar
- Drowned, Runner ve oyuncu gövdesi için Animation Blueprint'lerin **Parent Class**'ı `NLCreatureAnimInstance` olmalı. `Speed` / `bIsMoving` ile Idle↔Walk/Run geçişini yap.
- AnimGraph'ta state machine çıkışını **Slot 'DefaultSlot'** düğümünden geçir. Montajlar ancak böyle oynar.
- Montajları Details panelinden ata:
  - Drowned: Appear, **Cover Face** (elini yüzüne kapatma), Flee (kaçış), Jumpscare
  - Runner: Spawn, Kill
  - Player: Sit, Stand, Death
  - Generator: Player Repair / Refuel; FuseBox: Player Switch; FuelCan: Player Pickup
- Montaj atanmadığında kod Fallback süreleriyle çalışmaya devam eder, oyun bozulmaz.

## 5. HUD
`NLHUDWidget` sınıfından bir Widget Blueprint (WBP_NLHUD) yap. Aşağıdaki isimlerle widget ekle (hepsi opsiyonel):
`DayText, ClockText, InteractPromptText, FlashlightBar, FuelCanText, CameraPanel, NoPowerPanel, CameraNameText, FuelBar, FlashCooldownBar, GeneratorStatusText, RepairPanel, RepairProgressBar, RepairZone, RepairNeedle, CenterMessageText, GameOverPanel`

- `RepairZone` ve `RepairNeedle` Image olsun, sola hizalansın. RepairZone'un genişliği **1 px** olsun (kod onu X ekseninde ölçekleyerek bölgeyi çizer). Bar genişliği varsayılan 400 px (`RepairTrackWidth`).
- WBP'yi **`Content/UI/WBP_NLHUD`** yoluna kaydet. Kod onu otomatik olarak bulur.

### Kamera ekranı (CCTV görünümü)
Ekstra asset gerekmez, hepsi kodla kurulur:
- **Filtre:** Her `NLSecurityCamera`'nın Details > **NL|CCTV** bölümünde gri ton, kontrast, pozlama (karanlıkta görüntüyü aydınlatır), gren, vinyet, bloom ve renk kayması ayarları var. Görüntü çok karanlık ya da çok parlaksa önce **CCTV Exposure Bias** değerini değiştir.
- **Ekran üstü efektler:** karıncalanma, tarama çizgileri, aşağı kayan parlak bant, köşe kararması ve kamera değişince parazit. Yoğunlukları HUD'daki **NL|CCTV** ayarlarından değiştirilir.
- **Yerleşim:** sol üstte `CAM 1 KİLER`, sağ üstte gece ve kamera ızgarası, ortada nişangah (ışık hazırsa parlak, altındaki çizgi cooldown), sol altta pil (jeneratör benzini), sağ altta tarih ve saat.
- Kamera adını değiştirmek için kameranın **Location Label** alanını doldur (ör. `KİLER`). Boş bırakılırsa Camera Name kullanılır.
- Tarih **Feed Start Year/Month/Day** değerinden başlar, her gece bir gün ilerler.

## 6. Oyuncunun ses ve animasyonlarını Blueprint kullanmadan atamak
`NLPlayerCharacter`'ı sahneye sürükle ve Details panelinde **Auto Possess Player = Player 0** yap. Footstep sesini, Sit/Stand/Death montajlarını, mesh'i ve fener seslerini bu aktörün Details panelinden atayabilirsin. Böylece GameMode'un spawn ettiği boş karakter yerine bu karakter kullanılır.

## Kontroller
| Tuş | Ne yapar |
|---|---|
| WASD / Fare | Yürü / bak |
| E | Etkileşim (otur, kalk, tamir et, benzin al/doldur, şalter, fener şarjı) |
| SPACE | Masada: aktif kameranın ışığını yakar. Jeneratörde: tamir vuruşu |
| A/D, ←/→, 1-8 | Masada kamera değiştir |
| F | Fener |

## Test komutları
- Oyun sırasında konsolu (`~` tuşu) açıp `NLDebugRunner` yazarsan Runner hemen oyuncuyu kovalamaya başlar.
- `NLDebugFuseIn 5`: şalter 5 sn sonra kendiliğinden atar. `NLDebugTimeScale 20`: oyun zamanı 20 kat hızlanır.
- Runner'ın konumu oyuncuya NavMesh üzerinden bağlı olmalı. Alt kattaki odalar kapı yüzünden lobiye bağlı değil, o yüzden Runner koridorun doğu ucunda duruyor. Yeşil NavMesh alanını görmek için editörde **P** tuşuna bas.

## Oyuncu gövdesi ve pozlar
- Oyuncunun gövdesi artık görünüyor (UE mankeni, kafa gizli). Görünmeyen bir "sürücü" mesh yürüme animasyonunu oynatıyor, görünen gövde (`BodyRig`) bu pozu kopyalayıp kodla büküyor. Animasyon asset'i gerekmiyor.
- **Fener** sağ elde: kapalıyken kalça yanında aşağı bakar, açınca göğüs önüne kalkar ve bakılan yöne tutulur. Işık fenerin ucundan çıkar. Şarja takılıyken elde görünmez.
- **Benzin bidonu:** E'ye basınca karakter eğilir, sol eliyle alır ve taşır. Bidon jeneratöre dökülünce kaybolur.
- **Masa:** Karakter sandalyeye yürür, oturur, ellerini masaya koyar ve monitöre bakar. Kamera ekranı oturma bitince açılır. Kalkarken tersini yapar.
- Ayarlar: oyuncunun Details panelinde **NL|Body** (oturma, eğilme süreleri, gövde ofseti) ve BodyRig'in **NL|Body** (göz konumu) bölümleri.
- Pozları kontrol etmek için konsola `NLDebugShots` yaz: fener, eğilme, taşıma ve oturma pozlarının ekran görüntüleri `Saved/Screenshots` klasörüne alınır.
- **El hareketleri:** Şalterde el kolu tutup çeker (kol çekildiği anda döner). Şarj istasyonunda fener istasyonun üstüne bırakılır ya da oradan alınır. Jeneratörde bidon iki elle kaldırılıp eğilerek dökülür (benzin dökülmenin ortasında eklenir). Süreler: NLFuseBox **Switch Action Duration**, NLFlashlightCharger **Hand Action Duration**, NLGenerator **Refuel Action Duration**.
- **Masadan kalkma:** Ellerini masaya bastırıp öne eğilir, doğrulur ve bir adım geri çekilir (**Stand Duration**, **Stand Step Back**).

## Şalter arızası
Şalter gün içinde 2–4 dakikada bir kendiliğinden atar ve tüm elektrik gider. Ekranda "FUSE TRIPPED! RESET THE BREAKER" yazar, oyuncu şaltere gidip E ile kolu kaldırır. Ayarlar NLFuseBox'ta **Random Trips**, **Min/Max Trip Interval** (saniye). Sayaç sadece gün sürerken ve şalter açıkken ilerler, şalter tekrar açılınca baştan başlar.

## Boğulmuş: ışık tutulmazsa
Üst üste **2 kez** ışık tutulmazsa saldırır (NLDrownedDirector > **Misses Before Attack**). İlk kaçırmada çığlık sesiyle uyarır ve başka kameraya geçer. Başarılı bir ışık sayacı sıfırlar.

## Şalter kolu ve ek sesler
- Şalter kolu kutunun önünden çıkar: **açıkken yukarı, kapalıyken aşağı** bakar ve yumuşakça döner. Mafsal yeri ve açılar NLFuseBox'ta **Lever Pivot / Lever On Angle / Lever Off Angle**.
- Olaylara özel sesler `Scripts/nl_make_sounds.py` ile üretildi: `flashlight_on/off/empty`, `fuse_trip`, `charger_undock`, `drowned_warning/leave/return`, `runner_retreat`, `chair_sit/stand`, `player_death`. Gerçek kayıtlarla aynı isimle değiştirilebilir.
- `NLDebugShots2`: şalter kolu (açık / attı / tekrar açık) ve yürürken fener tutan elin ekran görüntülerini alır.

## Boğulmuş pozları
- Montaj atanmamışsa pozlar kodla verilir: ışık yiyince **iki eliyle yüzünü siper edip geriye eğilir ve titrer**, sonra **arkasını dönüp uzaklaşır**, jumpscare'de **kollarını iki yana açıp kameranın iki yanını kavrar, yüzünü öne atar; tutarken eller titrer ve oyuncunun bakışı sarsılır**. Gerçek montaj atarsan (Cover Face / Flee / Jumpscare) o oynar.
- Ayarlar: NLDrowned > **Flinch Lean Back**, **Flee Distance**, **Fallback Cover Face / Flee Duration**, **Jumpscare Lunge** (yüzün ne kadar yaklaştığı), **Jumpscare Camera Shake** (bakış sarsıntısı).
- Kaybolunca haritanın altına taşınır; böylece ışıkta gölgesi geride kalmaz.
- `NLDebugShots3`: güvenlik kamerasının gözünden belirme, yüz siperi, kaçış, kaybolma ve jumpscare görüntülerini alır.

## Runner jumpscare
- Runner yakalayınca Boğulmuş ile **aynı yakalama jumpscare'i** oynar: kameranın önüne atılır, kollarını iki yana açıp kameranın iki yanını kavrar, yüzünü öne atar, bakış sarsılır. Ortak kod: `NLJumpscareGrab`.
- Yakalanma anında oyuncu yere ya da tavana bakıyorsa bakış yataya düzeltilir, yüz hep tam karşıda olur (Boğulmuş için de geçerli).
- Runner'ın görünen gövdesi de artık `BodyRig`: koşarken kolları serbestçe sallanır. Gizlenince haritanın altına taşınır (gölge kalmaz).
- Ayarlar: NLRunner > **Jumpscare Distance / Lunge / Camera Shake**. Kill Montage atarsan onun yerine o oynar.
- `NLDebugShots4`: Runner kovalaması ve yakalama görüntüleri.

## Sunum: günü kısaltma
- **F9** (oyun sırasında): bu gün 5 sn sonra biter, "DAY 2" geçişi gösterilir. Sadece geliştirme sürümünde çalışır, paketlenmiş oyunda kapalıdır.
- Konsol: `NLDebugDayLength 5` → bundan sonra her gün 5 sn sürer (saat kaldığı yerden devam eder). `NLDebugDayLength 600` ile normale döner.
- Konsol: `NLDebugEndDayIn 5` → sadece bu gün 5 sn sonra biter.
- Oyunu kısa günlerle başlatmak: başlatma satırına `-NLDayLength=5` ekle (Editor Preferences > Play > Additional Launch Parameters ya da kısayolun hedefine).

## Arıza çakışması
Jeneratör arızası ve şalter atması aynı anda olmaz. Biri arızalıyken ya da düzeltildikten sonraki 45 sn içinde diğeri gelmez; sırası gelen arıza bu süre dolana kadar ertelenir (+5–20 sn rastgele). Süre `UNLPowerSubsystem::MinFailureGap`.
Test: `NLDebugFuseIn 4` + `NLDebugGeneratorIn 6` → şalter atar, jeneratör arızası ertelenir.
