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
| Shift (basılı) | Koş (stamina harcar) |
| E | Etkileşim (otur, kalk, tamir et, benzin al/doldur, şalter, fener şarjı) |
| SPACE | Masada: aktif kameranın ışığını yakar. Jeneratörde: tamir vuruşu |
| A/D, ←/→, 1-8 | Masada kamera değiştir |
| F | Fener |

## Test komutları
- Oyun sırasında konsolu (`~` tuşu) açıp `NLDebugRunner` yazarsan Runner hemen oyuncuyu kovalamaya başlar.
- `NLDebugFuseIn 5`: şalter 5 sn sonra kendiliğinden atar. `NLDebugTimeScale 20`: oyun zamanı 20 kat hızlanır.
- Runner'ın konumu oyuncuya NavMesh üzerinden bağlı olmalı. Runner koridorun doğu ucunda duruyor. Yeşil NavMesh alanını görmek için editörde **P** tuşuna bas.

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

## Etkileşim işaretleri
- Etkileşimli her nesnenin (masa, jeneratör, şalter, şarj, bidon) üstünde **7 m**'den itibaren yarı saydam koyu bir yuvarlak belirir; yaklaştıkça netleşir.
- Nesneye bakınca (E menzilinde) yuvarlak büyür, içinde **E** ve yanında eylem yazısı çıkar ("Switch power off" gibi). Yan yana nesnelerde sadece bakılanda E olur, diğerleri yuvarlak kalır.
- Bilgi yazılarında ("Generator - Fuel: 50%") E çıkmaz. Duvar arkasındaki, alınmış ya da kullanılamayan nesnelerde işaret görünmez. Masada, tamirde, eğilirken gizlenir.
- Ayarlar: HUD > **NL|Interact** (Marker Show Distance, Marker Size / Focused Size, Marker Height Offset, Marker Fill).
- `NLDebugShots5`: uzak/yakın işaretlerin arayüzlü ekran görüntülerini alır.

## Runner: tamir hatası
- Tamirde hata yapılınca Runner **uzakta** (oyuncudan 12–26 m, tercihen görmediği bir yerde) belirir ve oradan bir çığlık duyulur. Oyuncu **tamirden çıkana kadar bekler**, çıkınca koşmaya başlar.
- Koşu hızı **250** (oyuncu 280): kaçılabilir, ama durursan ya da sıkışırsan yakalar. Ayarlar: NLRunner > **Chase Speed**, **Spawn Min / Max Distance**.
- Kovalama **süresiz**: Runner ancak oyuncu bir odaya girip kapıyı kapatınca (aşağıda "Saklanma kapısı") ya da oyuncuyu yakalayınca durur. İstersen NLRunner > **Chase Duration** ile süre koyabilirsin (0 = süresiz).
- `NLDebugRunnerRepair`: tamir hatası senaryosunu baştan sona dener (log: `NLTest`).

## Sabotajcı
- Şalterin atması ve jeneratörün bozulması artık görünür bir karakterle oluyor: **Sabotajcı** oyuncunun görmediği bir yerde (hedefe 9–22 m) belirir, **çömelerek sinsice** hedefe yürür, hedefi elleriyle **kurcalar** (alçak hedefte eğilir) ve arıza o sırada olur; sonra **koşarak** görüş dışına kaçıp kaybolur.
- Sahneye konması gerekmez, ilk ihtiyaçta kendiliğinden oluşur. Koyu renkli manken (MI_Manny_02_New).
- O iş başındayken diğer arıza başlamaz; arıza çakışma kuralı (45 sn) bittiği andan itibaren sayılır.
- Hedefe 30 sn içinde ulaşamazsa arızayı yine uygular ve kaybolur (oyun akışı bozulmasın). Yol bulamazsa arıza eskisi gibi hemen olur.
- Ayarlar (NLSaboteur sınıfı varsayılanları): **Sneak Speed** 150, **Flee Speed** 520, **Tamper Duration** 2.6 sn, **Tamper Reach**, **Spawn Min/Max Distance**, **Sneak Crouch Depth**, **Give Up Time**.
- Test: `NLDebugFuseIn 1` / `NLDebugGeneratorIn 1` (Sabotajcı gelir), `NLDebugShots6` (yürüme, kurcalama, kaçış görüntüleri).

## Sigorta kutusu
- Şalter kutusunun paneli sola uzadı: üzerinde 2×2 dizili **4 dikey cam sigorta** (kırmızı, mavi, yeşil, sarı). Her yuvanın arkasındaki renkli plaka, oraya hangi sigortanın gittiğini gösterir.
- **Elektrik** ancak şalter açık **ve** 4 sigorta da doğru yuvadaysa gelir.
- **Sabotajcı** artık şalteri indirmez, sigortaların yerini karıştırır; şalteri bazen indirir, bazen açık bırakır (**Sabotage Switch Off Chance**, varsayılan %50).
- **E** → görüş kutunun **yakın plan kamerasına** kayar (kutudaki loş lamba yanar), fare imleci çıkar. Ayrı bir panel yok: **kutunun üstündeki sigortaya basılı tutup** panelin üstünde sürükle, başka yuvaya bırak → ikisi yer değiştirir (boşa bırakınca yerine döner; sadece tıklamak bir şey yapmaz, renkler doğru olsa da istediğin gibi değiştirebilirsin). **Şalter koluna basılı tutup** fareyi yukarı/aşağı çek: yarıyı geçip bırakınca o tarafa geçer, geçmezse geri döner. Altta sadece kısa bir ipucu satırı var. **E / Esc** ile geri dönülür.
- **Şalter açıkken sigortaya dokunulursa elektrik çarpar:** kamera hızla oyuncuya döner, ekran mavi-beyaz elektrikle cızırdar ve sallanır (oyuncu > **Shock Duration**, **Shock Shake**).
- Doğru sıra: şalteri kapat → sigortaları renklerine yerleştir → şalteri aç.
- `NLDebugFuseUI`: sabotaj, elektrik çarpması ve düzeltmenin ekran görüntülerini alır.
- `NLDebugFuseMouse`: gerçek fare girişiyle (imleç + sol tuş) sigorta sürükleme, tek tıklama ve şalter kolunu dener.

## Artı ve etkileşim odağı
- Ekranın ortasında küçük bir **artı** var; etkileşimli bir nesneye bakınca büyür ve parlar. Masada, sigorta kutusunda, tamirde gizlenir.
- **Odak daha rahat:** Nesnenin görünen sınırına **2,8 m**'den yakınsan ve artı nesnenin üstündeyse ya da ona yakınsa (küçük nesnelerde ~7° pay) E çıkar; dibine girince de çalışır. Arada duvar varsa seçilmez. Birden fazla aday varsa artıya en yakın olan seçilir.
- Bakılan nesnenin yuvarlağı ekran dışına düşerse (ör. dibindeyken) **E artının hemen üstünde** görünür.
- Ayarlar: oyuncu > **Interact Distance** (280), **Interact Aim Tolerance** (7°). Test: `NLDebugFocusShots`.

## Saklanma kapısı (Runner'dan kaçış)
- Alt kattaki depo odasının girişine (eski BP_DoorFrame yerine) **NLHideDoor_Storage** kondu (`nl_setup_scene.py`). Kapının yanı ve üstü dolgu duvarla kapanır, NavMesh kapıdan geçer.
- **E** ile kapı açılır/kapanır. Runner kovalarken odaya girip kapıyı **içeriden kapatırsan**:
  - Oyuncu kapıya yaslanır, **kamera kendiliğinden kapı koluna döner**, iki eli kapıda/kolda; **hızlı panik nefesi** (ses + kamera sallanması) başlar. Yürüme ve bakış kilitlenir.
  - Runner kapının dışına geçer ve **4 kez kapıyı zorlar**: her zorlamada darbe sesi, **kol aşağı basılıp titrer**, kapı aralanıp çarpar, kamera sarsılır ve ekranda **"HOLD THE DOOR!"** çubuğu çıkar. İbre bir kez soldan sağa geçer; **bölgedeyken SPACE**.
  - **Hepsi başarılı:** Runner bırakıp gider (uzaklaşan adımlar), nefes yavaşça söner, oyuncu serbest kalır.
  - **Bölge dışında basarsan ya da geç kalırsan:** kapı içeri savrulur, oyuncu geri itilir ve Runner **jumpscare** ile yakalar (karanlıkta yüzü alttan soluk bir ışıkla görünür).
- Ayarlar: NLHideDoor > **Num Checks** (4), **Settle Time**, **Gap Min / Max**, **Sweep Time** (ibre süresi, her kontrolde **Sweep Speed Up** kadar hızlanır), **Zone Width Start / Min**, **Burst Knockback**; ölçüler **Opening Width / Height**, **Wall Thickness**, **Door Width / Height**. Aktörün ileri oku (+X) odanın **içine** bakmalı.
- Oyuncu > **Breath Shake** (sallanma, derece) ve **Breath Rate** (nefes/sn).
- Yeni sesler (`nl_make_sounds.py`): door_open, door_close, door_bang, door_burst, door_handle, breath_panic_loop, breath_exhausted_loop.
- Test: `NLDebugDoor 1` (kontroller başarılı → Runner gider), `NLDebugDoor 0` (başarısız → kapı kırılır, jumpscare).

## Koşma ve stamina
- **Shift** basılıyken koşulur: hız **480** (yürüme 280, Runner 250). Tam stamina ile ~**5,5 sn** koşulur.
- Bırakınca ~0,9 sn sonra dolmaya başlar (dururken daha hızlı). Stamina **biterse** %30'a dolana kadar koşamazsın. Stamina azaldıkça nefes nefese kalma sesi duyulur.
- **Stamina bitince:** derin, hırıltılı nefes sesi başlar (`breath_exhausted_loop`) ve ekran her nefeste **gri → normal → gri** atar (içe çekişte renk çekilir ve hafif kararır, verişte geri gelir). %30'a dolup tekrar koşabilir hale gelince yavaşça normale döner. Ayarlar: oyuncu > **Exhausted Breath Loop**, **Exhausted Breath Period** (1,1 sn, sesle aynı), **Exhausted Saturation** (0,08).
- **Sıfırlama cezası:** stamina **tamamen biterse** bar **soluk gri** olur ve stamina **tamamen dolana kadar** daha yavaş dolar (normal hızın %45'i; oyuncu > **Depleted Regen Multiplier**). Bitmeden koşmayı bırakırsan normal hızda dolar. %30'dan sonra yine koşabilirsin ama yavaş dolma tam dolana kadar sürer.
- **Gösterge (alt orta):** ince, yıpranmış (çizik, düzensiz kenarlı) bir bar; stamina azaldıkça **iki yandan ortaya doğru kapanır**. Doluyken soluk kemik rengi, azaldıkça **griye** döner (kırmızı değil, can gibi durmasın) ve arızalı ışık gibi titrer. Stamina azaldıkça **ekran kenarları kararır** (tünel görüşü) ve nabız gibi atar; tükenince bar söner-yanar ve titrer. Dolu ve koşmuyorsan kaybolur. Tamamen kodla çiziliyor (`NLHUDStamina.cpp`), asset gerekmez.
- Ayarlar: oyuncu > **Sprint Speed**, **Sprint Duration**, **Stamina Regen Time / Delay**, **Exhausted Recover**, **Panting Loop**.
- Test: `NLDebugStamina` (Shift ile koşu + %75 / %35 / tükenmiş gösterge görüntüleri).
