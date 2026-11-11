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
