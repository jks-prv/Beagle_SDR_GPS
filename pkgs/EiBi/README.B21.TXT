How to use EiBi frequency lists.
================================

OVERVIEW
A) Conditions of use.
B) How to use.
C) Available formats.
D) Codes used.

================================


A) Conditions of use.

All my frequency lists are free of cost, and any person
is absolutely free to download, use, copy, or distribute
these files or to use them within third-party software.
Should any mistake, omission or misinformation be found
in my lists, please send your helpful comment to
Eike.Bierwirth (at) yandex.com
[replace the "(at)" by the @ sign]

Sources used to compile the files:
 - HFCC public data, found at http://www.hfcc.org/
 - Station schedules published on their own website or printed materials
 - Station schedules and observations distributed on mailing lists A-DX, DXLD, hard-core-dx, radioescutas, open_dx
 - Glenn Hauser's DX Listening Digest, found at http://www.w4uvh.net/dxlatest.txt
 - Wolfgang Büschel's Worldwide DX Club Top News, found at http://www.wwdxc.de/topnews.htm
 - Numbers & Oddities newsletters, found at http://www.numbersoddities.nl/newsletters.html
 - Own observations at home, on travel, and on remote Perseus receivers
 - In a few cases, Nagoya DX Circle's Bi Newsletter AOKI List, http://www1.s2.starcat.ne.jp/ndxc/ and http://www1.s2.starcat.ne.jp/ndxc/is/bi<season>.txt
 - Monitoring websites http://rri.jpn.org// and http://www.mcdxt.it/LASWLOGS.html
 - The licensing authority of Brazil, searchable at http://sistemas.anatel.gov.br/siscom/consplanobasico/default.asp

B) How to use.
   I) Try to identify a signal on the radio:
      You will need to know the frequency you tuned to in kHz, and the
      time of the reception in UTC.
      Search for the frequency in the list. Check all entries for this frequency:
       - The listening time should fall inbetween the start and end times given for the broadcast.
       - The day of the week should match. (If none is given in the list, the broadcast is daily.)
       - Try to guess the language you hear, if you don't know it.
       - Try to estimate the probability of propagation. The last column gives you information about
         the transmitter site (if not, the transmitter is in the home country of the station.)
         The target-area code should give you a clue if the signal is beamed in your direction or not.
         If you are right in the target area, great. If you are "behind" it (from the transmitter's
         point of view) or inbetween, your chances are also good. If the line connecting transmitter
         and target area is perpendicular to the line connecting the transmitter to your location,
         chances are lower. They are also lower if the target area is very close to the transmitter,
         while you are far away.
      Still, all rules can be turned upside-down by propagation conditions. You might sit in a good
      direction but the waves may skip 80 kilometers above you in the ionosphere. Listed information
      may be wrong, misleading, or outdated. Transmitters may fail, be upgraded, or drift in frequency.
      Transmitter sites may change (for example, fire in the control room, hurricane tore the antenna
      down, and transmissions have all been redirected to the Bonaire transmitter). Broadcasters and
      transmitter operators may mix up programmes or tune to the wrong frequency.
      My lists will be useful for fast identification of many stations on the band, but in a number of
      cases it will be of no use to you or it may bring you to a wrong conclusion. NO WARRANTY in case
      of a wrong ID, an embarrasingly wrong log or reception report sent out etc.
      
      The ONLY 100% ID is that which you hear on the radio yourself. Even QSLs may be wrong.
      
   II) Find a broadcast you would like to hear:
      Make extensive use of the SEARCH and SORT functions of the computer programme which you use.

      For text-format files I use TextPad, a freely downloadable text editor.
      Software has been written by very nice colleages of us to display the files in a more convenient manner,
      namely:
      
      ShortwaveLog, by Robert Sillett, N3OEA, which you can find on http://www.shortwavelog.com/
      
      RadioExplorer, by Dmitry Nefedov, Russia, which you can find on http://www.radioexplorer.com.ru/
      
      EiBiView, by Tobias Taufer, hosted on my website, real-time browsing!

      GuindaSoft Lister, freely downloadable from 
      http://guindasoft.impreseweb.com/utile/utile.html
      Click on the column headers to sort first by time, then by language or station or however you
      wish. Try it out.

      Not to forget the software of the PERSEUS receiver.

C) Available formats.

Since B06, the fundamental database is a CSV semicolon-separated list that contains
all the broadcasts relevant during the season, even if only for part of it.

A program script is used to transfer these data into the frequency-sorted and the
time-sorted text file. These two are the ones that exclusively contain the currently
valid data (current at the time of creation which is given at the top of the file).

Naming of files is
sked-Xzz.csv for the CSV database, load this into EiBiView;
freq-Xzz.txt for the frequency-sorted text file;
bc-Xzz.txt for the time-sorted text file;
where Xzz stands for the current broadcasting season:
 X=A: Summer season
 X=B: Winter season
 zz:  Two-character number for the year. E.g.,
 A99: Summer season 1999
 B06: Winter season 2006/2007.
 
The change from A to B or from B to A usually coincides with the
change of clock to or from Daylight Saving Time / Summer Time in many parts of
the world. The U.S.A., however, decided to use Daylight Saving Time
for an even longer period, beginning in 2007.

A file called eibi.txt is the same as the freq file, copy this into
your Perseus directory to view the EiBi entries in that receiver's software.

Format of the CSV database (sked-):
The entries are separated by semicolons (;), so each line has to contain ten semicolons - not more, not less.
Entry #1 (float between 10 and 30000): Frequency in kHz
Entry #2 (9-character string): Start and end time, UTC, as two '(I04)' numbers separated by a '-'
Entry #3 (string of up to 5 characters): Days of operation, or some special comments:
         Mo,Tu,We,Th,Fr,Sa,Su - Days of the week
         1245 - Days of the week, 1=Monday etc. (this example is "Monday, Tuesday, Thursday, Friday")
         1.Sa - First Saturday of the month
         1WeFr - First Wednesday of the month, repeated the following Friday
         Note: Vatican Radio counts major Catholic holidays as "Sunday"
         alt - Alternative frequency, not usually in use
         altFr - Alternating Fridays etc.
         harm - Harmonic signal (on multiples of the fundamental frequency)
         imod - Intermodulation signal (e.g., 17490 and 17650 on same transmitter produce 17330 and 17810)
         irr - Irregular operation
         Haj - Special broadcast for the Haj
         MF-15 - Monday through Friday, but only up the 15th day of each month
         Ram - Ramadan special schedule
         tent - Tentatively, please check and report your observations
         test - Test operation, may cease at any time
         15Sep - Broadcast only on 15 September
Entry #4 (string up to 3 characters): ITU code (see below) of the station's home country
Entry #5 (string up to 23 characters): Station name
Entry #6 (string up to 3 characters): Language code
Entry #7 (string up to 3 characters): Target-area code
Entry #8 (string): Transmitter-site code
Entry #9 (integer): Persistence code, where:
         1 = This broadcast is everlasting (i.e., it will be copied into next season's file automatically)
         2 = As 1, but with a DST shift (northern hemisphere)
         3 = As 1, but with a DST shift (southern hemisphere)
         4 = As 1, but active only in the winter season
         5 = As 1, but active only in the summer season
         6 = Valid only for part of this season; dates given in entries #10 and #11. Useful to re-check old logs.
         8 = Inactive entry, not copied into the bc and freq files.
Entry #10: Start date if entry #9=6. 0401 = 4th January. Sometimes used with #9=1 if a new permanent service is started, for information purposes only.
Entry #11: As 10, but end date. Additionally, the date of the most recent log can be noted in [brackets]. [0212]=Last heard in February 2012.


D) Codes used.
   I)   Language codes.
   II)  Country codes.
   III) Target-area codes.
   IV)  Transmitter-site codes.
   
   I) Language codes.
   
   Numbers are number of speakers. 4m = 4 millions.
   On the right in [brackets] the ISO 639-3 (SIL) language code for reference.
   For more information on language [abc] visit https://iso639-3.sil.org/code/abc
   
   
   -CW   Morse Station
   -EC   Empty Carrier
   -HF   HFDL Squitter (Aircraft comms station)
   -MX   Music
   -TS   Time Signal Station
   -TY   Teletype or other digital Station (RTTY/SITOR/..)
   A     Arabic (300m)                                                       [arb]
   AB    Abkhaz: Georgia-Abkhazia (0.1m)                                     [abk]
   AC    Aceh: Indonesia-Sumatera (3m)                                       [ace]
   ACH   Achang / Ngac'ang: Myanmar, South China (60,000)                    [acn]
   AD    Adygea / Adyghe / Circassian: Russia-Caucasus (0.5m)                [ady]
   ADI   Adi: India-Assam,Arunachal Pr. (0.1m)                               [adi]
   AF    Afrikaans: South Africa, Namibia (5m)                               [afr]
   AFA   Afar: Djibouti (0.3m), Ethiopia (0.45m), Eritrea (0.3m)             [aar]
   AFG   Pashto and Dari (main Afghan languages, see there)
   AH    Amharic: Ethiopia (22m)                                             [amh]
   AJ    Adja / Aja-Gbe: Benin, Togo (0.5m)                                  [ajg]
   AK    Akha: Burma (0.2m), China-Yunnan (0.13m)                            [ahk]
   AKL   Aklanon: Philippines-Visayas (0.5m)                                 [akl]
   AL    Albanian: Albania (Tosk)(3m), Macedonia/Yugoslavia (Gheg)(2m)       [sqi]
   ALG   Algerian (Arabic): Algeria (28m)                                    [arq]
   AM    Amoy: S China (25m), Taiwan (15m), SoEaAsia (5m); dialect of Minnan [nan]
   AMD   Tibetan Amdo (Tibet, Qinghai, Gansu, Sichuan: 2m)                   [adx]
   Ang   Angelus programme of Vatican Radio
   AR    Armenian: Armenia (3m), USA (1m), RUS,GEO,SYR,LBN,IRN,EGY           [hye]
   ARO   Aromanian/Vlach: Greece, Albania, Macedonia (0.1m)                  [rup]
   ARU   Languages of Arunachal, India (collectively)
   ASS   Assamese: India-Assam (13m)                                         [asm]
   ASY   Assyrian/Syriac/Neo-Aramaic: Iraq, Iran, Syria (0.2m)               [aii]
   ATS   Atsi / Zaiwa: Myanmar (13,000), China-Yunnan (70,000)               [atb]
   Aud   Papal Audience (Vatican Radio)
   AV    Avar: Dagestan, S Russia (0.7m)                                     [ava]
   AW    Awadhi: N&Ce India (3m)                                             [awa]
   AY    Aymara: Bolivia (2m)                                                [ayr]
   AZ    Azeri/Azerbaijani: Azerbaijan (6m)                                  [azj]
   BAD   Badaga: India-Tamil Nadu (0.13m)                                    [bfq]
   BAG   Bagri: India-Punjab (0.6m), Pakistan (0.2m)                         [bgq]
   BAI   Bai: China-Yunnan (1.2m)                                            [bca]
   BAJ   Bajau: Malaysia-Sabah (50,000)                                      [bdr]
   BAL   Balinese: Indonesia-Bali (3m)                                       [ban]
   BAN   Banjar/Banjarese: Indonesia-Kalimantan (3.5m)                       [bjn]
   BAO   Baoulé: Cote d'Ivoire (2m)                                          [bci]
   BAR   Bari: South Sudan (0.4m)                                            [bfa]
   BAS   Bashkir/Bashkort:  Russia-Bashkortostan (1m)                        [bak]
   BAY   Bayash/Boyash (gypsy dialect of Romanian): Serbia, Croatia (10,000)
   BB    Braj Bhasa/Braj Bhasha/Brij: India-Rajasthan (0.6m)                 [bra]
   BC    Baluchi: Pakistan (5m)                                              [bal]
   BE    Bengali/Bangla: Bangladesh (110m), India (82m)                      [ben]
   BED   Bedawiyet / Bedawi / Beja: Sudan (1m)                               [bej]
   BEM   Bemba: Zambia (3m)                                                  [bem]
   BET   Bete / Bété (Guiberoua): Ivory Coast (0.2m)                         [bet]
   BGL   Bagheli: N India (3m)                                               [bfy]
   BH    Bhili: India-Madhya Pradesh, Gujarat (3.3m)                         [bhb]
   BHN   Bahnar: Vietnam (160,000)                                           [bdq]
   BHT   Bhatri: India-Chhattisgarh,Maharashtra (0.2m)                       [bgw]
   BI    Bilen/Bile: Eritrea-Keren (90,000)                                  [byn]
   BID   Bidayuh languages: Malaysia-Sarawak (70,000)                        [sdo]
   BIS   Bisaya: Malaysia-Sarawak,Sabah (20,000), Brunei (40,000)            [bsb]
   BJ    Bhojpuri/Bihari: India (38m), Nepal (1.7m)                          [bho]
   BK    Balkarian: Russia-Caucasus (0.3m)                                   [krc]
   BLK   Balkan Romani: Bulgaria (0.3m), Serbia (0.1m), Macedonia (0.1m)     [rmn]
   BLT   Balti: NE Pakistan (0.3m)                                           [bft]
   BM    Bambara/Bamanankan/Mandenkan: Mali (4m)                             [bam]
   BNA   Borana Oromo/Afan Oromo: Ethiopia (4m)                              [gax]
   BNG   Bangala / Mbangala: Central Angola (0.4m)                           [mxg]
   BNI   Baniua/Baniwa: Brazil-Amazonas (6,000)                              [bwi]
   BNJ   Banjari / Banjara / Gormati / Lambadi: India (4m)                   [lmn]
   BNT   Bantawa: Nepal (400,000)                                            [bap]
   BON   Bondo: India-Odisha (9000)                                          [bfw]
   BOR   Boro / Bodo: India-Assam,W Bengal (1.3m)                            [brx]
   BOS   Bosnian (derived from Serbocroat): Bosnia-Hercegovina (2m)          [bos]
   BR    Burmese / Barma / Myanmar: Myanmar (32m)                            [mya]
   BRA   Brahui: Pakistan (4m), Afghanistan (0.2m)                           [brh]
   BRB   Bariba / Baatonum: Benin (0.5m), Nigeria (0.1m)                     [bba]
   BRU   Bru: Laos (30,000), Vietnam (55,000)                                [bru]
   BSL   Bislama: Vanuatu (10,000)                                           [bis]
   BT    Black Tai / Tai Dam: Vietnam (0.7m)                                 [blt]
   BTK   Batak-Toba: Indonesia-Sumatra (2m)                                  [bbc]
   BU    Bulgarian: Bulgaria (6m)                                            [bul]
   BUG   Bugis / Buginese: Indonesia-Sulawesi (5m)                           [bug]
   BUK   Bukharian/Bukhori: Israel (50,000), Uzbekistan (10,000)             [bhh]
   BUN   Bundeli / Bundelkhandi / Bundelkandi: India-Uttar,Madhya Pr. (3m)   [bns]
   BUR   Buryat: Russia-Buryatia, Lake Baikal (0.4m)                         [bxr]
   BUY   Bouyei/Buyi/Yay: China-Guizhou (2.6m), No.Vietnam. Close to ZH.     [pcc]
   BY    Byelorussian / Belarusian: Belarus, Poland, Ukraine (8m)            [bel]
   C     Chinese (not further specified)
   CA    Cantonese / Yue: China-Guangdong (50m),Hongkong (6m),Malaysia (1m)  [yue]
   CC    Chaochow (dialect of Min-Nan): China-Guangdong (10m), Thailand (1m) [nan]
   CD    Chowdary/Chaudhry/Chodri: India-Gujarat (0.2m)                      [cdi]
   CEB   Cebuano: Philippines (16m)                                          [ceb]
   CH    Chin (not further specified): Myanmar; includes those below a.o.
   C-A   Chin-Asho: Myanmar-Ayeyarwady,Rakhine (30,000)                      [csh]
   C-D   Chin-Daai: Myanmar-Chin (37,000)                                    [dao]
   C-F   Chin-Falam / Halam: Myanmar-Chin, Bangladesh, India (0.1m)          [cfm]
   C-H   Chin-Haka: Myanmar-Chin (100,000)                                   [cnh]
   CHA   Cha'palaa / Chachi: Ecuador-Esmeraldas (10,000)                     [cbi]
   CHE   Chechen: Russia-Chechnya (1.4m)                                     [che]
   CHG   Chhattisgarhi: India-Chhattisgarh, Odisha, Bihar (13m)              [hne]
   CHI   Chitrali / Khowar: NW Pakistan (0.2m)                               [khw]
   C-K   Chin-Khumi: Myanmar-Chin,Rakhine (0.6m)                             [cnk]
   C-M   Chin-Mro: Myanmar-Rakhine,Chin (75,000)                             [cmr]
   C-O   Chin-Thado / Thadou-Kuki: India-Assam, Manipur (0.2m)               [tcz]
   CHR   Chrau: Vietnam (7,000)                                              [crw]
   CHU   Chuwabu: Mozambique (1m)                                            [chw]
   C-T   Chin-Tidim: Myanmar-Chin (0.2m), India-Mizoram,Manipur (0.15m)      [ctd]
   C-Z   Chin-Zomin / Zomi-Chin: Myanmar (60,000), India-Manipur (20,000)    [zom]
   CKM   Chakma: India-Mizoram,Tripura,Assam (0.2m), Bangladesh (0.15m)      [ccp]
   CKW   Chokwe: Angola (0.5m), DR Congo (0.5m)                              [cjk]
   COF   Cofan / Cofán: Ecuador-Napo, Colombia (2000)                        [con]
   COK   Cook Islands Maori / Rarotongan: Cook Islands (13,000)              [rar]
   CR    Creole / Haitian: Haiti (7m)                                        [hat]
   CRU   Chru: Vietnam (19,000)                                              [cje]
   CT    Catalan: Spain (7m), Andorra (31,000)                               [cat]
   CV    Chuvash: Russia-Chuvashia (1m)                                      [chv]
   CVC   Chavacano/Chabacano: Spanish creole in PHL-Mindanao (4m)            [cbk]
   CW    Chewa/Chichewa/Nyanja/Chinyanja: Malawi (7m), MOZ (0.6m),ZMB (0.8m) [nya]
   CZ    Czech: Czech Republic (9m)                                          [ces]
   D     German: Germany (80m), Austria, Switzerland, Belgium                [deu]
   D-P   Lower German (varieties in N.Germany, USA:Pennsylvania Dutch)       [pdc]
   DA    Danish: Denmark (5.5m)                                              [dan]
   DAH   Dahayia: India
   DAO   Dao: Vietnam ethnic group speaking MIE and Kim Mun (0.7m)
   DAR   Dargwa/Dargin: Russia-Dagestan (0.5m)                               [dar]
   DD    Dhodiya / Dhodia: India-Gujarat (150,000)                           [dho]
   DEC   Deccan/Deccani/Desi: India-Maharashtra (13m)                        [dcc]
   DEG   Degar / Montagnard (Vietnam): comprises JR, RAD, BHN, KOH, MNO, STI
   DEN   Dendi: Benin (30,000)                                               [ddn]
   DEO   Deori: India-Assam (27,000)                                         [der]
   DES   Desiya / Deshiya: India-Odisha (50,000)                             [dso]
   DH    Dhivehi: Maldives (0.3m)                                            [div]
   DI    Dinka: South Sudan (1.4m)                           [dip,diw,dik,dib,dks]
   DIM   Dimasa/Dhimasa: India-Assam: (0.1m)                                 [dis]
   DIT   Ditamari: Benin (0.1m)                                              [tbz]
   DO    Dogri (sometimes includes Kangri dialect): N India (4m)     [doi,dgo,him]
   DR    Dari / Eastern Farsi: Afghanistan (7m), Pakistan (2m)               [prs]
   DU    Dusun: Malaysia-Sabah (0.1m)                                        [dtp]
   DUN   Dungan: Kyrgyzstan (40,000)                                         [dng]
   DY    Dyula/Jula: Burkina Faso (1m), Ivory Coast (1.5m), Mali (50,000)    [dyu]
   DZ    Dzongkha: Bhutan (0.2m)                                             [dzo]
   E     English: UK (60m), USA (225m), India (200m), others                 [eng]
   EC    Eastern Cham: Vietnam (70,000)                                      [cjm]
   EGY   Egyptian Arabic: Egypt (52m)                                        [arz]
   EO    Esperanto: Constructed language (2m)                                [epo]
   ES    Estonian: Estonia (1m)                                              [ekk]
   EWE   Ewe / Éwé: Ghana (2m), Togo (1m)                                    [ewe]
   F     French: France (53m), Canada (7m), Belgium (4m), Switzerland (1m)   [fra]
   FA    Faroese: Faroe Islands (66,000)                                     [fao]
   FI    Finnish: Finland (5m)                                               [fin]
   FJ    Fijian: Fiji (0.3m)                                                 [fij]
   FON   Fon / Fongbe: Benin (1.4m)                                          [fon]
   FP    Filipino (based on Tagalog): Philippines (25m)                      [fil]
   FS    Farsi / Iranian Persian: Iran (45m)                                 [pes]
   FT    Fiote / Vili: Rep. Congo (7000), Gabon (4000)                       [vif]
   FU    Fulani/Fulfulde: Nigeria (8m), Niger (1m),Burkina Faso (1m) [fub,fuh,fuq]
   FUJ   FutaJalon / Pular: Guinea (3m)                                      [fuf]
         Fujian: see TW-Taiwanese
   FUR   Fur: Sudan-Darfur (0.5m)                                            [fvr]
   GA    Garhwali: India-Uttarakhand,Himachal Pr. (3m)                       [gbm]
   GAG   Gagauz: Moldova (0.1m)                                              [gag]
   GAR   Garo: India-Meghalaya,Assam,Nagaland,Tripura (1m)                   [grt]
   GD    Greenlandic Inuktikut: Greenland (50,000)                           [kal]
   GE    Georgian: Georgia (4m)                                              [kat]
   GI    Gilaki: Iran (3m)                                                   [glk]
   GJ    Gujari/Gojri: NW India (0.7m), Pakistan (0.3m)                      [gju]
   GL    Galicic/Gallego: Spain (3m)                                         [glg]
   GM    Gamit: India-Gujarat (0.3m)                                         [gbl]
   GNG   Gurung (Eastern and Western): Nepal (0.4m)                      [ggn,gvr]
   GO    Gorontalo: Indonesia-Sulawesi (1m)                                  [gor]
   GON   Gondi: India-Madhya Pr.,Maharashtra (2m)                            [gno]
   GR    Greek: Greece (10m), Cyprus (0.7m)                                  [ell]
   GU    Gujarati: India-Gujarat,Maharashtra,Rajasthan (46m)                 [guj]
   GUA   Guaraní: Paraguay (5m)                                              [grn]
   GUN   Gungbe / Gongbe / Goun: Benin, Nigeria (0.7m)                       [guw]
   GUR   Gurage/Guragena: Ethiopia (0.4m)                                    [sgw]
   GZ    Ge'ez / Geez (liturgic language of Ethiopia)                        [gez]
   HA    Haussa: Nigeria (19m), Niger (5m), Benin (1m)                       [hau]
   HAD   Hadiya: Ethiopia (1.2m)                                             [hdy]
   HAR   Haryanvi /  Bangri / Harayanvi / Hariyanvi: India-Haryana (8m)      [bgc]
   HAS   Hassinya/Hassaniya: Mauritania (3m)                                 [mey]
   HB    Hebrew: Israel (5m)                                                 [heb]
   HD    Hindko (Northern and Southern): Pakistan (3m)                   [hnd,hno]
   HI    Hindi: India (260m)                                                 [hin]
   HIM   Himachali languages: India-Himachal Pradesh                         [him]
   HK    Hakka: South China (26m), Taiwan (3m), Malaysia (1m)                [hak]
         Hokkien: see TW-Taiwanese
   HM    Hmong/Miao languages: S China, N Vietnam, N Laos, USA (3m)          [hmn]
   HMA   Hmar: India-Assam,Manipur,Mizoram (80,000)                          [hmr]
   HMB   Hmong-Blue/Njua: Laos (0.1m), Thailand (60,000)                     [hnj]
   HMQ   Hmong/Miao, Northern Qiandong / Black Hmong: S China (1m)           [hea]
   HMW   Hmong-White/Daw: Vietnam (1m), Laos (0.2m), S China (0.2m)          [mww]
   HN    Hani: China-Yunnan (0.7m)                                           [hni]
   HO    Ho: India-Jharkand,Odisha,W Bengal (1m)                             [hoc]
   HR    Croatian/Hrvatski: Croatia (4m), BIH (0.5m), Serbia (0.1m)          [hrv]
   HRE   Hre: Vietnam (0.1m)                                                 [hre]
   HU    Hungarian: Hungary (10m), Romania (1.5m), SVK (0.5m), SRB (0.3m)    [hun]
   HUI   Hui / Huizhou: China-Anhui,Zhejiang (5m)                            [czh]
   HZ    Hazaragi: Afghanistan (1.8m), Iran (0.3m)                           [haz]
   I     Italian: Italy (55m), Switzerland (0.5m), San Marino (25,000)       [ita]
   IB    Iban: Malaysia-Sarawak (0.7m)                                       [iba]
   IBN   Ibanag: Philippines-Luzon (0.5m)                                    [ibg]
   IF    Ifè / Ife: Togo (0.1m), Benin (80,000)                              [ife]
   IG    Igbo / Ibo: Nigeria (18m)                                           [ibo]
   ILC   Ilocano: Philippines (7m)                                           [ilo]
   ILG   Ilonggo / Hiligaynon: Philippines-Visayas/Mindanao (9m)             [hil]
   IN    Indonesian / Bahasa Indonesia: Indonesia (140m)                     [ind]
   INU   Inuktikut: Canada-Nunavut,N Quebec,Labrador (30,000)                [ike]
   IRQ   Iraqi Arabic: Iraq (12m), Iran (1m), Syria (2m)                     [acm]
   IS    Icelandic: Iceland (0.2m)                                           [isl]
   ISA   Isan / Northeastern Thai: Thailand (15m)                            [tts]
   ITA   Itawis / Tawit: Philippines-Luzon (0.1m)                            [itv]
   J     Japanese: Japan (122m)                                              [jpn]
   JAI   Jaintia / Pnar / Synteng: India-Meghalaya, Bangladesh (250,000)     [pbv]
   JEH   Jeh: Vietnam (15,000), Laos (8,000)                                 [jeh]
   JG    Jingpho: see KC-Kachin
   JOR   Jordanian Arabic: Jordan (3.5m), Israel/Palestine (2.5m)            [ajp]
   JR    Jarai / Giarai / Jra: Vietnam (0.3m)                                [jra]
   JU    Juba Arabic: South Sudan (60,000)                                   [pga]
   JV    Javanese: Indonesia-Java,Bali (84m)                                 [jav]
   K     Korean: Korea (62m), China-Jilin,Heilongjiang,Liaoning (2m)         [kor]
   KA    Karen (unspecified): Myanmar (3m)
   K-G   Karen-Geba: Myanmar (40,000)                                        [kvq]
   K-K   Karen-Geko / Gekho: Myanmar (17,000)                                [ghk]
   K-M   Manumanaw Karen / Kawyaw / Kayah: Myanmar (10,000)                  [kxf]
   K-P   Karen-Pao / Black Karen / Pa'o: Myanmar (0.5m)                      [blk]
   K-S   Karen-Sgaw / S'gaw: Myanmar (1.3m), Thailand (0.2m)                 [ksw]
   K-W   Karen-Pwo: Myanmar (1m); Northern variant: Thailand (60,000)    [kjp,pww]
   KAD   Kadazan: Malaysia-Sabah (80,000)                                [kzj,dtb]
   KAL   Kalderash Romani (Dialect of Vlax): Romania (0.2m)                  [rmy]
   KAB   Kabardian: Russia-Caucasus (0.5m), Turkey (1m)                      [kbd]
   KAM   Kambaata: Ethiopia (0.6m)                                           [ktb]
   KAN   Kannada: India-Karnataka,Andhra Pr.,Tamil Nadu (40m)                [kan]
   KAO   Kaonde: Zambia (0.2m)                                               [kqn]
   KAR   Karelian: Russia-Karelia (25,000), Finland (10,000)                 [krl]
   KAT   Katu: Vietnam (50,000)                                              [ktv]
   KAU   Kau Bru / Kaubru/ Riang: India-Tripura,Mizoram,Assam (77,000)       [ria]
   KAY   Kayan: Myanmar (0.1m)                                               [pdu]
   KB    Kabyle: Algeria (5m)                                                [kab]
   KBO   Kok Borok/Tripuri: India (0.8m)                                     [trp]
   KC    Kachin / Jingpho: Myanmar (0.9m)                                    [kac]
   KG    Kyrgyz /Kirghiz: Kyrgystan (2.5m), China (0.1m)                     [kir]
   KGU   Kalanguya / Kallahan: Philippines-Luzon (0.1m)                      [kak]
   KH    Khmer: Cambodia (13m), Vietnam (1m)                                 [khm]
   KHA   Kham / Khams, Eastern: China-NE Tibet (1.4m)                        [khg]
   KHM   Khmu: Laos (0.6m)                                                   [kjg]
   KHR   Kharia / Khariya: India-Jharkand (0.2m)                             [khr]
   KHS   Khasi / Kahasi: India-Meghalaya,Assam (0.8m)                        [kha]
   KHT   Khota (India)
   KIM   Kimwani: Mozambique (0.1m)                                          [wmw]
   KIN   Kinnauri / Kinori: India-Himachal Pr. (65,000)                      [kfk]
   KiR   KiRundi: Burundi (9m)                                               [run]
   KK    KiKongo/Kongo: DR Congo, Angola (8m)                                [kng]
   KKA   Kankana-ey: Philippines-Luzon (0.3m)                                [kne]
   KKN   Kukna: India-Gujarat (0.1m)                                         [kex]
   KKU   Korku: India-Madhya Pr.,Maharashtra (1m)                            [kfq]
   KMB   Kimbundu/Mbundu/Luanda: Angola (4m)                                 [kmb]
   KMY   Kumyk: Russia-Dagestan (0.4m)                                       [kum]
   KND   Khandesi: India-Maharashtra (22,000)                                [khn]
   KNG   Kangri: close to Dogri; India-Himachal Pradesh, Punjab (1m)         [xnr]
   KNK   KinyaRwanda-KiRundi service of the Voice of America / BBC
   KNU   Kanuri: Nigeria (3.2m), Chad (0.1m), Niger (0.4m)                   [kau]
   KNY   Konyak Naga: India-Assam,Nagaland (0.25m)                           [nbe]
   KOH   Koho/Kohor: Vietnam (0.2m)                                          [kpm]
   KOK   Kokang Shan: Myanmar (dialect of Shan)
   KOM   Komering: Indonesia-Sumatera (0.5m)                                 [kge]
   KON   Konkani: India-Maharashtra,Karnataka,Kerala (2.4m)                  [knn]
   KOR   Korambar / Kurumba Kannada: India-Tamil Nadu (0.2m)                 [kfi]
   KOT   Kotokoli / Tem: Togo (0.2m), Benin (0.05m), Ghana (0.05m)           [kdh]
   KOY   Koya: India-Andhra Pr.,Odisha (0.4m)                                [kff]
   KPK   Karakalpak: W Uzbekistan (0.4m)                                     [kaa]
   KRB   Karbi / Mikir / Manchati: India-Assam,Arunachal Pr. (0.4m)          [mjw]
   KRI   Krio: Sierra Leone (0.5m)                                           [kri]
   KRW   KinyaRwanda: Rwanda (7m), Uganda (0.4m), DR Congo (0.2m)            [kin]
   KRY   Karay-a: Philippines-Visayas (0.4m)                                 [krj]
   KS    Kashmiri: India (5m), Pakistan (0.1m)                               [kas]
   KT    Kituba (simplified Kikongo): Rep. Congo (1m), DR Congo (4m)         [ktu]
   KTW   Kotwali (dialect of Bhili): India-Gujarat,Maharshtra                [bhb]
   KU    Kurdish: Turkey (15m), Iraq (6.3m), Iran (6.5m), Syria (1m) [ckb,kmr,sdh]
   KuA   Kurdish and Arabic
   KuF   Kurdish and Farsi
   KUI   Kui: India-Odisha,Ganjam,Andhra Pr. (1m)                            [kxu]
   KUL   Kulina: Brazil-Acre (3500)                                          [cul]
   KUM   Kumaoni/Kumauni: India-Uttarakhand (2m)                             [kfy]
   KUN   Kunama: Eritrea (0.2m)                                              [kun]
   KUP   Kupia / Kupiya: India-Andhra Pr. (6,000)                            [key]
   KUR   Kurukh/Kurux: India-Chhatisgarh,Jharkhand,W.Bengal (2m)             [kru]
   KUs   Sorani (Central) Kurdish: Iraq (3.5m), Iran (3.3m)                  [ckb]
   KUT   Kutchi: India-Gujarat (0.4m), Pakistan-Sindh (0.2m)                 [gjk]
   KUV   Kuvi: India-Odisha (0.16m)                                          [kxv]
   KVI   Kulluvi/Kullu: India-Himachal Pr. (0.1m)                            [kfx]
   KWA   Kwanyama/Kuanyama (dialect of OW): Angola (0.4m), Namibia (0.2m)    [kua]
   KYH   Kayah: Myanmar (0.15m)                                              [kyu]
   KZ    Kazakh: Kazakhstan (7m), China (1m), Mongolia (0.1m)                [kaz]
   L     Latin: Official language of Catholic church                         [lat]
   LA    Ladino: see SEF
   LAD   Ladakhi / Ladak: India-Jammu and Kashmir (0.1m)                     [lbj]
   LAH   Lahu: China (0.3m), Myanmar (0.2m)                                  [lhu]
   LAK   Lak: Russia-Dagestan (0.15m)                                        [lbe]
   LAM   Lampung: Indonesia-Sumatera (1m)                                [abl,ljp]
   LAO   Lao: Laos (3m)                                                      [lao]
   LB    Lun Bawang / Murut: Malaysia-Sarawak (24,000), Indonesia (23,000)   [lnd]
   LBN   Lebanon Arabic (North Levantine): Lebanon (4m), Syria (9m)          [apc]
   LBO   Limboo /Limbu: Nepal (0.3m), India-Sikkim,W.Bengal,Assam (40,000)   [lif]
   LEP   Lepcha: India-Sikkim,W.Bengal (50,000)                              [lep]
   LEZ   Lezgi: Russia-Dagestan (0.4m), Azerbaijan (0.4m)                    [lez]
   LIM   Limba: Sierra Leone (0.3m)                                          [lia]
   LIN   Lingala: DR Congo (2m), Rep. Congo (0.1m)                           [lin]
   LIS   Lisu: China-West Yunnan (0.6m), Burma (0.3m)                        [lis]
   LND   Lunda (see LU), in particular its dialect Ndembo: Angola (0.2m)     [lun]
   LNG   Lungeli Magar (possibly same as MGA?)
   LO    Lomwe / Ngulu: Mocambique (1.5m)                                    [ngl]
   LOK   Lokpa / Lukpa / Lupka: Benin (50,000), Togo (14,000)                [dop]
   LOZ   Lozi / Silozi: Zambia (0.6m), ZWE (70,000), NMB-E Caprivi (30,000)  [loz]
   LT    Lithuanian: Lithuania (3m)                                          [lit]
   LTO   Oriental Liturgy of Vatican Radio
   LU    Lunda: Zambia (0.5m)                                                [lun]
   LUB   Luba: DR Congo-Kasai (6m)                                           [lua]
   LUC   Luchazi: Angola (0.4m), Zambia (0.05m)                              [lch]
   LUG   Luganda: Uganda (4m)                                                [lug]
   LUN   Lunyaneka/Nyaneka: Angola (0.3m)                                    [nyk]
   LUR   Luri, Northern and Southern: Iran (1.5m and 0.9m)               [lrc,luz]
   LUV   Luvale: Angola (0.5m), Zambia (0.2m)                                [lue]
   LV    Latvian: Latvia (1.2m)                                              [lvs]
   M     Mandarin (Standard Chinese / Beijing dialect): China (840m)         [cmn]
   MA    Maltese: Malta (0.3m)                                               [mlt]
   MAD   Madurese/Madura: Indonesia-Java (7m)                                [mad]
   MAG   Maghi/Magahi/Maghai: India-Bihar,Jharkhand (14m)                    [mag]
   MAI   Maithili / Maithali: India-Bihar (30m), Nepal (3m)                  [mai]
   MAK   Makonde: Tanzania (1m), Mozambique (0.4m)                           [kde]
   MAL   Malayalam: India-Kerala (33m)                                       [mal]
   MAM   Maay / Mamay / Rahanweyn: Somalia (2m)                              [ymm]
   MAN   Mandenkan (dialect continuum of BM, DY, MLK): Mali, BFA, CTI, GUI   [man]
   MAO   Maori: New Zealand (60,000)                                         [mri]
   MAR   Marathi: India-Maharashtra (72m)                                    [mar]
   MAS   Maasai/Massai/Masai: Kenya (0.8m), Tanzania (0.5m)                  [mas]
   MC    Macedonian: Macedonia (1.4m), Albania (0.1m)                        [mkd]
   MCH   Mavchi/Mouchi/Mauchi/Mawchi: India-Gujarat,Maharashtra (0.1m)       [mke]
   MEI   Meithei/Manipuri/Meitei: India-Manipur,Assam (1.5m)                 [mni]
   MEN   Mende: Sierra Leone (1.5m)                                          [men]
   MEW   Mewari/Mewadi (a Rajasthani variety): India-Rajasthan (5m)          [mtr]
   MGA   Magar (Western and Eastern): Nepal (0.8m)                       [mrd,mgp]
   MIE   Mien / Iu Mien: S China (0.4m), Vietnam (0.4m)                      [ium]
   MIS   Mising: India-Assam,Arunachal Pr. (0.5m)                            [mrg]
   MKB   Minangkabau: Indonesia-West Sumatra (5.5m)                          [min]
   MKS   Makassar/Makasar: Indonesia-South Sulawesi (2m)                     [mak]
   MKU   Makua / Makhuwa: Mocambique (3m)                                    [vmw]
   ML    Malay / Baku: Malaysia (10m), Singapore (0.4m), Indonesia (5m)  [zsm,zlm]
   MLK   Malinke/Maninka (We/Ea): Guinea (3m), SEN (0.4m), Mali (0.8m)   [emk,mlq]
   MLT   Malto / Kumarbhag Paharia: India-Jharkhand (12,000)                 [kmj]
   MNA   Mina / Gen: Togo (0.2m), Benin (0.1m)                               [gej]
   MNB   Manobo / T'duray: Philippines-Mindanao (0.1m)                       [mno]
   MNE   Montenegrin (quite the same as SR): Montenegro (0.2m)               [srp]
   MNO   Mnong (Ea,Ce,So): Vietnam (90,000), Cambodia (40,000)       [mng,cmo,mnn]
   MO    Mongolian: Mongolia (Halh; 2m), China (Peripheral; 3m)          [khk,mvf]
   MON   Mon: Myanmar-Mon,Kayin (0.7m), Thailand (0.1m)                      [mnw]
   MOO   Moore/Mòoré/Mossi: Burkina Faso (5m)                                [mos]
   MOR   Moro/Moru/Muro: Sudan-S Korodofan (30,000)                          [mor]
   MR    Maronite / Cypriot Arabic: Cyprus (1300)                            [acy]
   MRC   Moroccan/Mugrabian Arabic: Morocco (20m)                            [ary]
   MRI   Mari: Russia-Mari (0.8m)                                            [chm]
   MRU   Maru / Lhao Vo: Burma-Kachin,Shan (0.1m)                            [mhx]
   MSY   Malagasy: Madagaskar (16m)                                          [mlg]
   MUN   Mundari: India-Jharkhand,Odisha (1.1m)                              [unr]
   MUO   Muong: Vietnam (1m)                                                 [mtq]
   MUR   Murut: Malaysia-Sarawak,Sabah (4500)                        [kxi,mvv,tih]
   MV    Malvi: India-Madhya Pradesh, Rajasthan (6m)                         [mup]
   MW    Marwari (a Rajasthani variety): India-Rajasthan,Gujarat (6m)    [mwr,rwr]
   MX    Macuxi/Macushi: Brazil (16,000), Guyana (1,000)                     [mbc]
   MY    Maya (Yucatec): Mexico (0.7m), Belize (6000)                        [yua]
   MZ    Mizo / Lushai: India-Mizoram (0.7m)                                 [lus]
   NAG   Naga (var.incl. Ao,Makware): India-Nagaland, Assam (2m) [njh,njo,nmf,nph]
   NAP   Naga Pidgin / Bodo / Nagamese: India-Nagaland (30,000)              [nag]
   NDA   Ndau: Mocambique (1.6m), Zimbabwe (0.8m)                            [ndc]
   NDE   Ndebele: Zimbabwe (1.5m), South Africa-Limpopo (0.6m)           [nde,nbl]
   NE    Nepali/Lhotshampa: Nepal (11m), India (3m), Bhutan (0.1m)           [npi]
   NG    Nagpuri / Sadani / Sadari / Sadri: India-Jharkhand,W.Bengal (3m)    [sck]
   NGA   Ngangela/Nyemba: Angola (0.2m)                                      [nba]
   NIC   Nicobari: India-Nicobar Islands (40,000)                            [caq]
   NIG   Nigerian Pidgin: Nigeria (30m)                                      [pcm]
   NIS   Nishi/Nyishi: India-Arunachal Pradesh (0.2m)                        [njz]
   NIU   Niuean: Niue (2,000)                                                [niu]
   NJ    Ngaju Dayak: Indonesia-Borneo (0.9m)                                [nij]
   NL    Dutch: Netherlands (16m), Belgium (6m), Suriname (0.2m)             [nld]
   NLA   Nga La / Matu Chin: Myanmar-Chin (30,000), India-Mizoram (10,000)   [hlt]
   NO    Norwegian: Norway (5m)                                              [nor]
   NOC   Nocte / Nockte: India-Assam,Arunachal Pr. (33,000)                  [njb]
   NP    Nupe: Nigeria (0.8m)                                                [nup]
   NTK   Natakani / Netakani / Varhadi-Nagpuri: India-Maharashtra,M.Pr. (7m) [vah]
   NU    Nuer: Sudan (0.8m), Ethiopia (0.2m)                                 [nus]
   NUN   Nung: Vietnam (1m)                                                  [nut]
   NW    Newar/Newari: Nepal (0.8m)                                          [new]
   NY    Nyanja: see CW-Chichewa which is the same
   OG    Ogan: Indonesia-Sumatera (less than 0.5m)                           [pse]
   OH    Otjiherero service in Namibia (Languages: Herero, SeTswana)
   OO    Oromo: Ethiopia (26m)                                               [orm]
   OR    Odia / Oriya / Orissa: India-Odisha,Chhattisgarh (32m)              [ory]
   OS    Ossetic: Russia (0.5m), Georgia (0.1m)                              [oss]
   OW    Oshiwambo service in Angola and Namibia (Languages: Ovambo, Kwanyama)
   P     Portuguese: Brazil (187m), Angola (14m), Portugal (10m)             [por]
   PAL   Palaung - Pale: Myanmar (0.3m)                                      [pce]
   PAS   Pasemah: Indonesia-Sumatera (less than 0.5m)                        [pse]
   PED   Pedi: S Africa (4m)                                                 [nso]
   PJ    Punjabi: Pakistan (60m), India-Punjab,Rajasthan (28m)           [pnb,pan]
   PO    Polish: Poland (37m)                                                [pol]
   POR   Po: Myanmar-Rakhine (identical to K-W?)
   POT   Pothwari: Pakistan (2.5m)                                           [phr]
   PS    Pashto / Pushtu: Afghanistan (6m), Pakistan (1m)                    [pbt]
   PU    Pulaar: Senegal (3m), Gambia (0.3m)                                 [fuc]
   Q     Quechua: Peru, Bolivia, Ecuador (various varieties; 9m)         [que,qvi]
   QQ    Qashqai: Iran (1.5m)                                                [qxq]
   R     Russian: Russia (137m), Ukraine (8m), Kazakhstan (6m), Belarus (1m) [rus]
   RAD   Rade/Ede: Vietnam (0.2m)                                            [rad]
   RAJ   Rajasthani: common lingua franca in Rajasthan (18m)                 [raj]
   REN   Rengao: Vietnam (18,000)                                            [ren]
   RGM   Rengma Naga: India-Nagaland (34,000)                            [nre,nnl]
   RO    Romanian: Romania (20m), Moldova (3m), Serbia-Vojvodina (0.2m)      [ron]
   ROG   Roglai (Northern, Southern): Vietnam (0.1m)                     [rog,rgs]
   ROH   Rohingya (rjj): Myanmar-Rakhine (2m)                                [rhg]
   RON   Rongmei Naga: India-Manipur,Nagaland,Assam (60,000)                 [nbu]
   Ros   Rosary session of Vatican Radio
   RU    Rusyn / Ruthenian: Ukraine (0.5m), Serbia-Vojvodina (30,000)        [rue]
   RWG   Rawang: Myanmar-Kachin (60,000)                                     [raw]
   S     Spanish/Castellano: Spain (30m), Latin America (336m), USA (34m)    [spa]
   SAH   Saho: Eritrea (0.2m)                                                [ssy]
   SAN   Sango: Central African Rep. (0.4m)                                  [sag]
   SAR   Sara/Sar: Chad (0.2m)                                               [mwm]
   SAS   Sasak: Indonesia-Lombok (2m)                                        [sas]
   SC    Serbocroat (Yugoslav language up to national/linguistic separation) [hbs]
   SCA   Scandinavian languages (Norwegian, Swedish, Finnish)
   SD    Sindhi: Pakistan (19m), India (2m)                                  [snd]
   SED   Sedang: Vietnam (0.1m)                                              [sed]
   SEF   Sefardi/Judeo Spanish/Ladino: Israel (0.1m), Turkey (10,000)        [lad]
   SEN   Sena: Mocambique (1m)                                               [seh]
   SFO   Senoufo/Sénoufo-Syenara: Mali (0.15m)                               [shz]
   SGA   Shangaan/Tsonga: Mocambique (2m), South Africa (2m)                 [tso]
   SGM   Sara Gambai / Sara Ngambai: Chad (0.9m)                             [sba]
   SGO   Songo: Angola (50,000)                                              [nsx]
   SGT   Sangtam: India-Nagaland (84,000)                                    [nsa]
   SHA   Shan: Myanmar (3m)                                                  [shn]
   SHk   Shan-Khamti: Myanmar (8,000), India-Assam (5,000)                   [kht]
   SHC   Sharchogpa / Sarchopa / Tshangla: E Bhutan (0.14m)                  [tsj]
   SHE   Sheena/Shina: Pakistan (0.6m)                                   [scl,plk]
   SHK   Shiluk/Shilluk: South Sudan (0.2m)                                  [shk]
   SHO   Shona: Zimbabwe (11m)                                               [sna]
   SHP   Sherpa: Nepal (0.1m)                                                [xsr]
   SHU   Shuwa Arabic: Chad (1m), Nigeria (0.1m), N Cameroon (0.1m)          [shu]
   SI    Sinhalese/Sinhala: Sri Lanka (16m)                                  [sin]
   SID   Sidamo/Sidama: Ethiopia (3m)                                        [sid]
   SIK   Sikkimese/Bhutia: India-Sikkim,W.Bengal (70,000)                    [sip]
   SIR   Siraiki/Seraiki: Pakistan (14m)                                     [skr]
   SK    Slovak: Slovakia (5m), Czech Republic (0.2m), Serbia (80,000)       [slk]
   SLM   Pijin/Solomon Islands Pidgin: Solomon Islands (0.3m)                [pis]
   SLT   Silte / East Gurage / xst: Ethiopia (1m)                            [stv]
   SM    Samoan: Samoa (0.2m), American Samoa (0.05m)                        [smo]
   SMP   Sambalpuri / Sambealpuri: India-Odisha,Chhattisgarh (18m)           [spv]
   SNK   Sanskrit: India (0.2m)                                              [san]
   SNT   Santhali: India-Bihar,Jharkhand,Odisha (6m), Bangladesh (0.2m)      [sat]
   SO    Somali: Somalia (8m), Ethiopia (5m), Kenya (2m), Djibouti (0.3m)    [som]
   SON   Songhai: Mali (0.6m)                                            [ses,khq]
   SOT   SeSotho: South Africa (4m), Lesotho (2m)                            [sot]
   SR    Serbian: Serbia (7m), Bosnia-Hercegovina (1.5m)                     [srp]
   SRA   Soura / Sora: India-Odisha,Andhra Pr. (0.3m)                        [srb]
   STI   Stieng: Vietnam (85,000)                                        [sti,stt]
   SUA   Shuar: Ecuador (35,000)                                             [jiv]
   SUD   Sudanese Arabic: Sudan and South Sudan (15m)                        [apd]
   SUM   Sumi Naga: India-Nagaland (0.1m)                                    [nsm]
   SUN   Sunda/Sundanese: Indonesia-West Java (34m)                          [sun]
   SUR   Surgujia: India-Chhattisgarh (1.5m)                                 [sgj]
   SUS   Sudan Service of IBRA, in SUD, BED, FUR, and TGR
   SV    Slovenian: Slovenia (1.7m), Italy (0.1m), Austria (18,000)          [slv]
   SWA   Swahili/Kisuaheli: Tanzania (15m), Kenya, Ea.DR Congo (9m)      [swc,swh]
   SWE   Swedish: Sweden (8m), Finland (0.3m)                                [swe]
   SWZ   SiSwati: Swaziland (1m), South Africa (1m)                          [ssw]
   T     Thai: Thailand (20m)                                                [tha]
   TAG   Tagalog: Philippines (22m)                                          [tgl]
   TAH   Tachelhit/Sous: Morocco, southern (4m), Algeria                     [shi]
   TAL   Talysh: Azerbaijan, Iran (1m)                                       [tly]
   TAM   Tamil: S.India (60m), Malaysia (4m), Sri Lanka (4m)                 [tam]
   TAU   Tausug: Philippines-Sulu, n.Borneo (1m)                             [tsg]
   TB    Tibetan / Lhasa Tibetan: Tibet (1m), India (0.1m)                   [bod]
   TBL   Tboli / T'boli / Tagabili: Philippines-Mindanao (0.1m)              [tbl]
   TBS   Tabasaran: Russia-Dagestan (0.1m)                                   [tab]
   TEL   Telugu: India-Andhra Pr. (74m)                                      [tel]
   TEM   Temme/Temne: Sierra Leone (1.5m)                                    [tem]
   TFT   Tarifit: Morocco, northern (1.3m), Algeria                          [rif]
   TGB   Tagabawa / Bagobo: Philippines-Mindanao (43,000)                    [bgs]
   TGK   Tangkhul/Tangkul Naga: India-Manipur,Nagaland (0.15m)               [nmf]
   TGR   Tigre/Tigré/Tigrawit: Eritrea (1m)                                  [tig]
   TGS   Tangsa/Naga-Tase: Myanmar (60,000), India-Arunachal Pr. (40,000)    [nst]
   THA   Tharu Buksa: India-Uttarakhand (43,000)                             [tkb]
   TIG   Tigrinya/Tigray: Ethiopia (4m), Eritrea (3m)                        [tir]
   TJ    Tajik: Tajikistan (3m), Uzbekistan (1m)                             [tgk]
   TK    Turkmen: Turkmenistan (3m), Iran (2m), Afghanistan (1.5m)           [tuk]
   TKL   Tagakaulo (dialect of Kalagan): Philippines-Mindanao (0.2m)     [kqe,klg]
   TL    Tai-Lu/Lu: China-Yunnan (0.3m), Myanmar (0.2m), Laos (0.1m)         [khb]
   TM    Tamazight: Morocco, central (3m)                                    [zgh]
   TMG   Tamang: Nepal (1.5m)                                    [taj,tdg,tmk,tsf]
   TMJ   Tamajeq: Niger (0.8m), Mali (0.44m), Algeria (40,000)   [taq,thv,thz,ttq]
   TN    Tai-Nua/Chinese Shan: China-Yunnan (0.5m), LAO/MYA/VTN (0.2m)       [tdd]
   TNG   Tonga: Zambia (1m), Zimbabwe (0.1m)                                 [toi]
   TO    Tongan: Tonga (0.1m)                                                [ton]
   TOK   Tokelau: Tokelau (1000)                                             [tkl]
   TOR   Torajanese/Toraja: Indonesia-Sulawesi (0.8m)                        [sda]
   TP    Tok Pisin: Papua New Guinea (4m)                                    [tpi]
   TS    Tswana / SeTswana: Botswana (1m), South Africa (3m)                 [tsn]
   TSA   Tsangla: see SHC
   TSH   Tshwa: Mocambique (1m)                                              [tsc]
   TT    Tatar: Russia-Tatarstan,Bashkortostan (5m)                          [tat]
   TTB   Tatar-Bashkir service of Radio Liberty
   TU    Turkish: Turkey (46m), Bulgaria (0.6m), N Cyprus (0.2m)             [tur]
   TUL   Tulu: India-Karnataka,Kerala (2m)                                   [tcy]
   TUM   Tumbuka: Malawi (2m), Zambia (0.5m)                                 [tum]
   TUN   Tunisian Arabic: Tunisia (9m)                                       [aeb]
   TUR   Turkana: NW Kenya (1m)                                              [tuv]
   TV    Tuva / Tuvinic: Russia-Tannu Tuva (0.25m)                           [tyv]
   TW    Taiwanese/Fujian/Hokkien/Min Nan (CHN 25m, TWN 15m, others 9m)      [nan]
   TWI   Twi/Akan: Ghana (8m)                                                [aka]
   TWT   Tachawit/Shawiya/Chaouia: Algeria (1.4m)                            [shy]
   TZ    Tamazight/Berber: Morocco (2m)                                  [zgh,tzm]
   UA    Uab Meto / Dawan / Baikenu: West Timor (1m)                         [aoz]
   UD    Udmurt: Russia-Udmurtia (0.3m)                                      [udm]
   UI    Uighur: China-Xinjiang (8m), Kazakhstan (0.3m)                      [uig]
   UK    Ukrainian: Ukraine (32m), Kazakhstan (0.9m), Moldova (0.6m)         [ukr]
   UM    Umbundu: Angola (6m)                                                [umb]
   UR    Urdu: Pakistan (104m), India (51m)                                  [urd]
   UZ    Uzbek: Uzbekistan (16m)                                             [uzn]
   V     Vasco / Basque / Euskera: Spain (0.6m), France (76,000)             [eus]
   VAD   Vadari / Waddar / Od: India-Andhra Pr. (0.2m)                       [wbq]
   VAR   Varli / Warli: India-Maharashtra (0.6m)                             [vav]
   Ves   Vespers (Vatican Radio)
   Vn    Vernacular = local language(s)
   VN    Vietnamese: Vietnam (66m)                                           [vie]
   VV    Vasavi: India-Maharashtra,Gujarat (1m)                              [vas]
   VX    Vlax Romani / Romanes / Gypsy: Romania (0.2m), Russia (0.1m)        [rmy]
   W     Wolof: Senegal (4m)                                                 [wol]
   WA    Wa / Parauk: South China (0.4m), Myanmar (0.4m)                     [prk]
   WAO   Waodani/Waorani: Ecuador (2000)                                     [auc]
   WE    Wenzhou: dialect of WU
   WT    White Tai / Tai Don: Vietnam (0.3m), Laos (0.2m)                    [twh]
   WU    Wu: China-Jiangsu,Zhejiang (80m)                                    [wuu]
   XH    Xhosa: South Africa (8m)                                            [xho]
   YAO   Yao/Yawo: Malawi (2m), Mocambique (0.5m), Tanzania (0.4m)           [yao]
   YER   Yerukula: India-Andhra Pr. (70,000)                                 [yeu]
   YI    Yi / Nosu: China-Sichuan (2m)                                       [iii]
   YK    Yakutian / Sakha: Russia-Sakha (0.5m)                               [sah]
   YO    Yoruba: Nigeria (20m), Benin (0.5m)                                 [yor]
   YOL   Yolngu/Yuulngu: Australia-Northern Territory (4000)                 [djr]
   YUN   Dialects/languages of Yunnan (China)
   YZ    Yezidi program (Kurdish-Kurmanji language)
   Z     Zulu: South Africa (10m), Lesotho (0.3m)                            [zul]
   ZA    Zarma/Zama: Niger (2m)                                              [dje]
   ZD    Zande: DR Congo (0.7m), South Sudan (0.35m)                         [zne]
   ZG    Zaghawa: Chad (87,000), Sudan (75,000)                              [zag]
   ZH    Zhuang: Southern China, 16 varieties (15m)                          [zha]
   ZWE   Languages of Zimbabwe
         Zomi-Chin: see Chin-Zomi (C-Z)

   II) Country codes.
   Countries are referred to by their ITU code (cf. itu.int "List of all geographical area designations")
   Asterisks (*) denote non-official abbreviations
   
   ABW  Aruba
   AFG  Afghanistan
   AFS  South Africa
   AGL  Angola
   AIA  Anguilla
   ALB  Albania
   ALG  Algeria
   ALS  Alaska
   AMS  Saint Paul & Amsterdam Is.
   AND  Andorra
   AOE  Western Sahara
   ARG  Argentina
   ARM  Armenia
   ARS  Saudi Arabia
   ASC  Ascension Island
   ATA  Antarctica
   ATG  Antigua and Barbuda
   ATN  Netherlands Leeward Antilles (dissolved in 2010)
   AUS  Australia
   AUT  Austria
   AZE  Azerbaijan
   AZR  Azores
   B    Brasil
   BAH  Bahamas
   BDI  Burundi
   BEL  Belgium
   BEN  Benin
   BER  Bermuda
   BES  Bonaire, St Eustatius, Saba (Dutch islands in the Caribbean)
   BFA  Burkina Faso
   BGD  Bangla Desh
   BHR  Bahrain
   BIH  Bosnia-Herzegovina
   BIO  Chagos Is. (Diego Garcia) (British Indian Ocean Territory)
   BLM  Saint-Barthelemy
   BLR  Belarus
   BLZ  Belize
   BOL  Bolivia
   BOT  Botswana
   BRB  Barbados
   BRU  Brunei Darussalam
   BTN  Bhutan
   BUL  Bulgaria
   BVT  Bouvet
   CAB  Cabinda *
   CAF  Central African Republic
   CAN  Canada
   CBG  Cambodia
   CEU  Ceuta *
   CG7  Guantanamo Bay
   CHL  Chile
   CHN  China (People's Republic)
   CHR  Christmas Island (Indian Ocean)
   CKH  Cook Island
   CLA  Clandestine stations *
   CLM  Colombia
   CLN  Sri Lanka
   CME  Cameroon
   CNR  Canary Islands
   COD  Democratic Republic of Congo (capital Kinshasa)
   COG  Republic of Congo (capital Brazzaville)
   COM  Comores
   CPT  Clipperton
   CPV  Cape Verde Islands
   CRO  Crozet Archipelago
   CTI  Ivory Coast (Côte d'Ivoire)
   CTR  Costa Rica
   CUB  Cuba
   CUW  Curacao
   CVA  Vatican State
   CYM  Cayman Islands
   CYP  Cyprus
   CZE  Czech Republic
   D    Germany
   DJI  Djibouti
   DMA  Dominica
   DNK  Denmark
   DOM  Dominican Republic
   E    Spain
   EGY  Egypt
   EQA  Ecuador
   ERI  Eritrea
   EST  Estonia
   ETH  Ethiopia
   EUR  Iles Europe & Bassas da India *
   F    France
   FIN  Finland
   FJI  Fiji
   FLK  Falkland Islands
   FRO  Faroe Islands
   FSM  Federated States of Micronesia
   G    United Kingdom of Great Britain and Northern Ireland
   GAB  Gabon
   GEO  Georgia
   GHA  Ghana
   GIB  Gibraltar
   GLP  Guadeloupe
   GMB  Gambia
   GNB  Guinea-Bissau
   GNE  Equatorial Guinea
   GPG  Galapagos *
   GRC  Greece
   GRD  Grenada
   GRL  Greenland
   GTM  Guatemala
   GUF  French Guyana
   GUI  Guinea
   GUM  Guam / Guahan
   GUY  Guyana
   HKG  Hong Kong, part of China
   HMD  Heard & McDonald Islands
   HND  Honduras
   HNG  Hungary
   HOL  The Netherlands
   HRV  Croatia
   HTI  Haiti
   HWA  Hawaii
   HWL  Howland & Baker
   I    Italy
   ICO  Cocos (Keeling) Island
   IND  India
   INS  Indonesia
   IRL  Ireland
   IRN  Iran
   IRQ  Iraq
   ISL  Iceland
   ISR  Israel
   IW   International Waters
   IWA  Ogasawara (Bonin, Iwo Jima) *
   J    Japan
   JAR  Jarvis Island
   JDN  Juan de Nova *
   JMC  Jamaica
   JMY  Jan Mayen *
   JON  Johnston Island
   JOR  Jordan
   JUF  Juan Fernandez Island *
   KAL  Kaliningrad *
   KAZ  Kazakstan / Kazakhstan
   KEN  Kenya
   KER  Kerguelen
   KGZ  Kyrgyzstan
   KIR  Kiribati
   KNA  St Kitts and Nevis
   KOR  Korea, South (Republic)
   KOS  Kosovo
   KRE  Korea, North (Democratic People's Republic)
   KWT  Kuwait
   LAO  Laos
   LBN  Lebanon
   LBR  Liberia
   LBY  Libya
   LCA  Saint Lucia
   LIE  Liechtenstein
   LSO  Lesotho
   LTU  Lithuania
   LUX  Luxembourg
   LVA  Latvia
   MAC  Macao
   MAF  St Martin
   MAU  Mauritius
   MCO  Monaco
   MDA  Moldova
   MDG  Madagascar
   MDR  Madeira
   MDW  Midway Islands
   MEL  Melilla *
   MEX  Mexico
   MHL  Marshall Islands
   MKD  Macedonia (F.Y.R.)
   MLA  Malaysia
   MLD  Maldives
   MLI  Mali
   MLT  Malta
   MNE  Montenegro
   MNG  Mongolia
   MOZ  Mozambique
   MRA  Northern Mariana Islands
   MRC  Morocco
   MRN  Marion & Prince Edward Islands
   MRT  Martinique
   MSR  Montserrat
   MTN  Mauritania
   MWI  Malawi
   MYA  Myanmar (Burma) (also BRM)
   MYT  Mayotte
   NCG  Nicaragua
   NCL  New Caledonia
   NFK  Norfolk Island
   NGR  Niger
   NIG  Nigeria
   NIU  Niue
   NMB  Namibia
   NOR  Norway
   NPL  Nepal
   NRU  Nauru
   NZL  New Zealand
   OCE  French Polynesia
   OMA  Oman
   PAK  Pakistan
   PAQ  Easter Island
   PHL  Philippines
   PHX  Phoenix Is.
   PLM  Palmyra Island
   PLW  Palau
   PNG  Papua New Guinea
   PNR  Panama
   POL  Poland
   POR  Portugal
   PRG  Paraguay
   PRU  Peru
   PRV  Okino-Tori-Shima (Parece Vela) *
   PSE  Palestine
   PTC  Pitcairn
   PTR  Puerto Rico
   QAT  Qatar
   REU  La Réunion
   ROD  Rodrigues
   ROU  Romania
   RRW  Rwanda
   RUS  Russian Federation
   S    Sweden
   SAP  San Andres & Providencia *
   SDN  Sudan
   SEN  Senegal
   SEY  Seychelles
   SGA  South Georgia Islands *
   SHN  Saint Helena
   SLM  Solomon Islands
   SLV  El Salvador
   SMA  Samoa (American)
   SMO  Samoa
   SMR  San Marino
   SNG  Singapore
   SOK  South Orkney Islands *
   SOM  Somalia
   SPM  Saint Pierre et Miquelon
   SRB  Serbia
   SRL  Sierra Leone
   SSD  South Sudan
   SSI  South Sandwich Islands *
   STP  Sao Tome & Principe
   SUI  Switzerland
   SUR  Suriname
   SVB  Svalbard *
   SVK  Slovakia
   SVN  Slovenia
   SWZ  Swaziland
   SXM  Sint Maarten
   SYR  Syria
   TCA  Turks and Caicos Islands
   TCD  Tchad
   TGO  Togo
   THA  Thailand
   TJK  Tajikistan
   TKL  Tokelau
   TKM  Turkmenistan
   TLS  Timor-Leste
   TON  Tonga
   TRC  Tristan da Cunha
   TRD  Trinidad and Tobago
   TUN  Tunisia
   TUR  Turkey
   TUV  Tuvalu
   TWN  Taiwan *
   TZA  Tanzania
   UAE  United Arab Emirates
   UGA  Uganda
   UKR  Ukraine
   UN   United Nations *
   URG  Uruguay
   USA  United States of America
   UZB  Uzbekistan
   VCT  Saint Vincent and the Grenadines
   VEN  Venezuela
   VIR  American Virgin Islands
   VRG  British Virgin Islands
   VTN  Vietnam
   VUT  Vanuatu
   WAK  Wake Island
   WAL  Wallis and Futuna
   XBY  Abyei area
   XGZ  Gaza strip
   XSP  Spratly Islands
   XUU  Unidentified
   XWB  West Bank
   YEM  Yemen
   ZMB  Zambia
   ZWE  Zimbabwe

   III) Target-area codes.
   Af  - Africa
   Am  - America(s)
   As  - Asia
   C.. - Central ..
   Car - Caribbean, Gulf of Mexico, Florida Waters
   Cau - Caucasus
   CIS - Commonwealth of Independent States (Former Soviet Union)
   CNA - Central North America
   E.. - East ..
   ENA - Eastern North America
   ENE - East-northeast
   ESE - East-southeast
   Eu  - Europe (often including North Africa/Middle East)
   FE  - Far East
   Glo - Global
   In  - Indian subcontinent
   LAm - Latin America (=Central and South America)
   ME  - Middle East
   N.. - North ..
   NAO - North Atlantic Ocean
   NE  - Northeast
   NNE - North-northeast
   NNW - North-northwest
   NW  - Northwest
   Oc  - Oceania (Australia, New Zealand, Pacific Ocean)
   S.. - South ..
   SAO - South Atlantic Ocean
   SE  - Southeast
   SEA - South East Asia
   SEE - South East Europe
   Sib - Siberia
   SSE - South-southeast
   SSW - South-southwest
   SW  - Southwest
   Tib - Tibet
   W.. - West ..
   WIO - Western Indian Ocean
   WNA - Western North America
   WNW - West-northwest
   WSW - West-southwest
   
   Alternatively, ITU country codes may be used if the target area is limited to
   a certain country. This is often the case for domestic stations.
   
   IV) Transmitter site codes.
   One-letter or two-letter codes for different transmitter sites within one country.
   No such code is used if there is only one transmitter site in that country.
   
   The code is used plainly if the transmitter is located within the home country of the station.
   Otherwise, it is preced by "/ABC" where ABC is the ITU code of the host country of the transmitter site.
   Example: A BBC broadcast, relayed by the transmitters in Samara, Russia, would be designated as "/RUS-s".
   
   In many cases, a station has a "major" transmitter site which is normally not designated. Should this station
   use a different transmitter site in certain cases, they are marked.
   No transmitter-site code is used when the transmitter site is not known.
   
   Resources for transmitter sites include:
    For Brazil: http://sistemas.anatel.gov.br/siscom/consplanobasico/default.asp
    For USA and territories: http://transition.fcc.gov/ib/sand/neg/hf_web/stations.html
    Google Earth and Wikimapia
   Many more details can be discussed in an online group dedicated to BC SW transmitter sites: http://groups.yahoo.com/group/shortwavesites/
    
   
   AFG: k-Kabul / Pol-e-Charkhi 34N32-69E20
        x-Khost 33N14-69E49
        y-Kabul-Yakatut 34N32-69E13
   AFS: Meyerton 26S35-28E08 except:
        ct-Cape Town 33S41-18E42
        j-Johannesburg 26S07'40"-28E12'20"
   AGL: L-Luena (Moxico) 11S47'00"-19E55'19"
        lu-Luanda Radio 08S48-13E16
        m-Mulenvos 08S51-13E19
   AIA: The Valley 18N13-63W01
   ALB: c-Cerrik (CRI) 41N00-20E00
        f-Fllake (Durres, 500kW) 41N22-19E30
        s-Shijiak (Radio Tirana) (1x100kW = 2x50kW) 41N20-19E33
   ALG: ad-Adrar 27N52-00W17
        al-Algiers 36N46-03E03
        an-Annaba 36N54-07E46
        b-Béchar 31N34-02W21
        o-Ourgla 31N55-05E04
        or-Oran 7TO 35N46-00W33
        r-Reggane 26N42-00E10
        s-In Salah (Ain Salih) 27N15-02E31
        t-Tindouf (Rabbouni) 27N33-08W06
        tm-Timimoun 29N16-00E14
   ALS: an-Annette 55N03-131W34
        ap-Anchor Point 59N44'58"-151W43'56"
        ba-Barrow 71N15'30"-156W34'39"
        cb-Cold Bay 55N13-162W43
        e-Elmendorf AFB 61N15'04"-149W48'23"
        g-Gakona 62N23'30"-145W08'48"
        k-Kodiak 57N46'30"-152W32
        ks-King Salmon 58N41-156W40
        no-Nome 64N31-165W25
   ARG: b-Buenos Aires 34S37'19"-58W21'18"
        co-Córdoba 31S18-64W14
        cr-Comodoro Rivadavia 45S51-67W25
        g-General Pacheco 34S36-58W22
        mp-Mar del Plata, Prov. Buenos Aires 38S03-57W32
   ARM: Gavar (formerly Kamo) 40N25-45E12
        y-Yerevan 40N10-44E30
   ARS: Riyadh 24N30-46E23 except:
        j-Jeddah/Jiddah 21N15-39E10
        jr-Jiddah Radio 21N23-39E10
        jz-Jazan 16N53-42E34
        nj-Najran 17N30-44E08
   ASC: Ascension Island, 07S54-14W23
   ATA: e-Base Esperanza 63S24-57W00
        f-Bahia Fildes, King George Island 62S12-58W58
   AUS: a-Alice Springs NT 23S49-133E51
        ae-Aero sites: Cape Pallarenda 19S12'05"-146E46'05" and Broken Hill 31S55'38"-141E28'57" and Knuckeys Lagoon 12S25'52"-130E57'51"
        as-Alice Springs NT 23S46'48"-133E52'28"
        at-Alice Springs NT 23S46'45"-133E52'25"
        b-Brandon QL 19S31-147E20
        bm-Broadmeadows VIC 37S41'31"-144E56'44"
        c-Charleville QL 26S25-146E15
        ex-Exmouth WA 21S49-114E10
        g-Gunnedah NSW 30S59-150E15
        h-Humpty Doo NT 12S34-131E05
        hc-Halls Creek NSW, 50 km NE of Tamsworth
        in-Innisfail QL 17S32-146E03
        ka-Katherine NT 14S24-132E11
        ku-Kununurra WA 15S49-128E40 (24 Jul 2012 moved to "Lot 3000" 2 mi west)
        L-Sydney-Leppington NSW 33S58-150E48
        m-Macleay Island QL 27S37-153E21
        ma-Manilla NSW 30S44'23"-150E42'56"
        n-Ningi QL 27S04'00"-153E03'20"
        rm-Roma QL 26S33-148E48
        rz-Razorback NSW 34S09-150E40
        s-Shepparton VIC 36S20-145E25
        sf-Schofields, western Sydney 33S42-150E52
        sm-St Mary's, Sydney 33S45-150E46
        t-Tennant Creek NT 19S40-134E16
        w-Wiluna WA 26S20-120E34
        ww-Wee Waa NSW 30S12'55"-149E27'26"
   AUT: Moosbrunn 48N00-16E28
   AZE: b-Baku 40N28-50E03
        g-Gäncä 40N36-46E20
        s-Stepanakert 39N49'35"-46E44'23"
   AZR: ho-Horta 38N32-28W38
        lj-Lajes Field 38N46-27E05
        sm-Santa Maria 36N56'50"-25W09'30"
   B:   a-Porto Alegre, RS 30S01'25"-51W15'19"
        ag-Araguaína, TO 07S12-48W12
        am-Amparo, SP 22S42-46W46
        an-Anápolis, GO 16S15'25"-49W01'08"
        ap-Aparecida, SP 22S50'47"-45W13'13"
        ar-Araraquara, SP 21S48-48W11
        b-Brasilia, Parque do Rodeador, DF 15S36'40"-48W07'53"
        be-Belém, PA 01S27-48W29
        br-Braganca, PA 01S03'48"-46W46'24"
        bt-Belém, PA (Ondas Tropicais 5045) 01S22-48W21
        bv-Boa Vista, RR 02N55'19"-60W42'38"
        c-Contagem/Belo Horizonte, MG 19S53'59"-44W03'16"
        ca-Campo Largo (Curitiba), PR 25S25'48"-49W23'49"
        cb-Camboriú, SC 27S02'25"-48W39'17"
        cc-Cáceres, MT 16S04'36"-57W38'27"
        cg-Campo Grande, MS 20S31'12"-54W35'00"
        Cg-Campo Grande, MS 20S27-54W37
        cl-Cabedelo, PB
        cm-Campinas, SP 22S56'52"-47W01'05"
        cn-Congonhas, MG 20S30-43W52
        co-Coari, AM 04S06'59"-63W07'31"
        cp-Cachoeira Paulista, SP 22S38'45"-45W04'42"
        Cp-Cachoeira Paulista, SP 22S38'39"-45W04'38"
        cs-Cruzeiro do Sul, Estrada do Aeroporto, AC 07S38-72W40
        cu-Curitiba, PR 25S27'08"-49W06'50"
        cv-Cuiabá, MT 15S37'07"-56W05'52"
        c2-Curitiba, PR RB2 25S23'34"-49W10'04"
        E-Esteio (Porto Alegre), RS 29S49'41"-51W09'54"
        e-Esteio (Porto Alegre), RS 29S51'59"-51W06'11"
        f-Foz do Iguacu, PR 25S31'03"-54W30'30"
        fl-Florianópolis, SC 27S36'09"-48W31'51"
        fp-Florianópolis - Comboriú, SC 27S02'24"-48W39'17"
        g-Guarujá, SP 23S59'35"-46W15'23"
        gb-Guaíba (Porto Alegre), RS 29S59'50"-51W17'08"
        gc-Sao Gabriel de Cachoeira, AM 00S08-67W05
        gm-Guajará-Mirim, RO 10S47-65W20
        go-Goiânia, 16S39'30"-49W13'38"
        gu-Guarulhos, SP 23S26-46W25
        h-Belo Horizonte, MG 19S58'34"-43W56'00"
        ib-Ibitinga, SP 21S46'20"-48W50'10"
        it-Itapevi, SP 23S30'39"-46W40'34"
        ld-Londrina, PR 23S20'16"-51W13'18"
        li-Limeira, SP 22S33'39"-47W25'08"
        lj-Lajeado, RS 29S28-51W58
        lo-Londrina, PR 23S24'17"-51W09'19"
        m-Manaus AM 03S06-60W02
        ma-Manaus - Radiodif.Amazonas, AM 03S08'16"-59W58'53"
        mc-Macapá, AP 00N03'50"-51W02'20"
        mg-Manaus - Radio Globo, AM 03S08'04"-59W58'39"
        mi-Marília, SP 22S13'33"-49W57'46"
        mm-São Mateus do Maranhão, Maranhão 04S02-44W28
        mo-Mogi das Cruces, SP 23S30'55"-46W12'08"
        mr-Manaus - Radio Rio Mar, AM 03S07'18"-60W02'30"
        ob-Óbidos, PA 01S55-55W31
        os-Osasco, SP 23S30'51"-46W35'39"
        pa-Parintins, AM 02S37-56W45
        pc-Pocos da Caldas, MG 21S47'52"-46W32'26"
        pe-Petrolina, PE 09S24-40W30
        pi-Piraquara (Curitiba), PR 25S23'34"-49W10'04"
        pv-Porto Velho, RO 08S47'45"-63W46'38" (4 dipolos em quadrado)
        r-Rio de Janeiro (Radio Globo), RJ 22S49'24"-43W05'49"
        rb-Rio Branco, AC 09S58-67W49
        rc-Rio de Janeiro (Radio Capital), RJ 22S46'43"-43W00'56"
        re-Recife, PE 08S04-34W58
        rj-Rio de Janeiro (Radio Relogio), RJ 22S46'41"-42W59'02"
        ro-Rio de Janeiro, Observatório Nacional, 22S53'45"-43W13'27"
        rp-Ribeirão Preto, SP 21S11-47W48
        rs-Rio de Janeiro (Super Radio), RJ 22S49'22"-43W05'21"
        rw-Rio de Janeiro PWZ 22S57-42W55
        sa-Santarém, PA 02S26'55"-54W43'58"
        sb-Sao Paulo - Radio Bandeirantes, SP 23S38'54"-46W36'02"
        sc-Sao Paulo - Radio Cultura, SP 23S30'42"-46W33'41"
        se-Senador Guiomard, AC 10S03-67W37
        sg-Sao Paulo - Radio Globo, SP 23S36'26"-46W26'12"
        sj-Sao Paulo - Radio 9 de Julho, SP 23S32'51"-46W38'10"
        sm-Santa Maria, RS 29S44'18"-53W33'19"
        sr-Sao Paulo - Radio Record, SP 23S41'02"-46W44'35"
        sy-Sao Paulo PYB45 23S33-46W38
        sz-Sao Paulo - Radio Gazeta, SP 23S40'10"-46W45'00"
        ta-Taubaté, SP 23S01-45W34
        te-Teresina, PI 05S05'13"-42W45'39"
        tf-Tefé, AM 03S21'15"-64W42'41"
        vi-Vitória, ES 20S19-40W19
        x-Xapuri, AC 10S39-68W30
        xm-Unknown location in Maranhão, 02S30-44W15
        xp-Unknown location in Paraíba, 07S10-36W50
        xx-Unknown location
   BEL: o-Oostende 51N11-02E48
        w-Wingene 51N11-02E49
   BEN: c-Cotonou 06N28-02E21
        p-Parakou 09N20-02E38
   BER: h-Bermuda Harbour 32N23-64W41
   BES: Bonaire 12N12-68W18
   BFA: Ouagadougou 12N26-01W33
   BGD: d-Dhaka-Dhamrai 23N54-90E12
        k-Dhaka-Khabirpur 24N00-90E15
        s-Dhaka-Savar 23N52-90E16
   BHR: a-Abu Hayan 26N02-50E37
        m-Al Muharraq 26N16-50E39
   BIH: b-Bijeljina 44N42-19E10
        z-Zavidovici 44N26-18E09
   BIO: Diego Garcia 07S26-72E26
   BLR: Minsk-Sasnovy/Kalodziscy 53N58-27E47 except:
        b-Brest 52N18-23E54
        g-Hrodna/Grodno 53N40-23E50
        m-Mahiliou/Mogilev ("Orsha") 53N37-30E20
        mo-Molodechno 54N28-26E47
        vi-Vitebsk 55N08-30E21
        xx-Unknown site(s)
   BOL: ay-Santa Ana del Yacuma 13S45-65W32
        cb-Cochabamba 17S23-66W11
        p-La Paz 16S30-68W08
        ri-Riberalta 10S59'49"-66W04'01"
        sc-Santa Cruz 17S48-63W10
        sz-Santa Cruz Airport 17S40-63W08
        uy-Uyuni 20S28-66W49
        yu-Yura 20S04-66W08
   BOT: Mopeng Hill 21S57-27E39
   BTN: Thimphu 27N28-89E39
   BUL: bg-Blagoevgrad (864) 42N03-23E03
        do-Doulovo (1161) 43N49-27E09
        kj-Kardjali (963) 41N36-25E22
        p-Plovdiv-Padarsko 42N23-24E52
        pe-Petrich 41N28-23E20
        s-Sofia-Kostinbrod 42N49-23E13
        sa-Samuil (864) 43N32'06"-26E44'13"
        sl-Salmanovo (747) 43N11-26E58
        tv-Targovishte (1161) 43N15-26E31
        va-Varna 43N04-27E47
        vi-Vidin 43N50-22E43
        vk-Vakarel (261) 42N34-23E42
   CAF: ba-Bangui 04N21-18E35
        bo-Boali 04N39-18E12
   CAN: al-Aldergrove BC, Matsqui Tx site 49N06'30"-122W14'40"
        ap-Amphitrite Point 48N55'31"-125W32'25"
        c-Calgary AB 50N54'02"-113W52'33"
        cb-Cambridge Bay 69N06'53"-105W01'11"
        cc-Churchill MB 58N45'42"-93W56'39"
        ch-Coral Harbour NU 64N09'01"-83W22'22"
        cr-Cap des Rosiers 48N51'40"-64W12'53"
        cw-Cartwright NL 53N42'30"-57W01'17"
        fg-CFG8525 ON 43N52-79W19
        g-Gander NL 48N57-54W37
        h-Halifax NS 44N41'03"-63W36'35"
        hd-Hopedale NL 55N27'24"-60W12'30"
        hp-Hunter Point 53N15'31"-132W42'53" and Digby Island 54N17'51"-130W25'06"
        hr-Hay River 60N50'27"-115W46'12"
        hx-Halifax CFH NS 44N57'50"-63W58'55"
        i-Iqaluit NU 63N43'42"-68W33'00"
        in-Inuvik NWT 68N19'33"-133W35'53"
        j-St John's NL 47N34'10"-52W48'52"
        k-Killiniq/Killinek NU 60N25'27"-64W50'30"
        lp-Lockeport NS 43N39'49"-65W07'47"
        lv-La Vernière, Îles-de-la-Madeleine QC 47N21'26"-61W55'36"
        na-Natashquan QC 50N08'40"-61W48'00"
        o-Ottawa ON 45N17'41"-75W45'29"
        pc-Port Caledonia NS 46N11'14"-59W53'59"
        r-Resolute NU 74N45'14"-94W58'09"
        sa-St Anthony NL 51N30'00"-55W49'26"
        sj-St John's NL 47N36'40"-52W40'01"
        sl-St Lawrence NL 46N55'09"-55W22'45"
        sm-Sambro NS 44N28'21"-63W37'13"
        sv-Stephenville NL 48N33'17"-58W45'32"
        t-Toronto (Mississauga/Clarkson) ON 43N30'23"-79W38'01"
        tr-Trenton 43N50'39"-77W08'47"
        v-Vancouver BC 49N08'21"-123W11'44"
        ym-Yarmouth/Chebogue NS 43N44'39"-66W07'21"
   CBG: ka-Kandal 11N25-104E50
   CHL: e-Radio Esperanza 38S41-72W35
        gc-"General Carrera" (RCW), unknown location
        pa-Punta Arenas 52S57-71W04
        s-Santiago (Calera de Tango) 33S38'36"-70W51'02"
        t-Talagante 33S40-70W56
        v-Valparaiso 32S48-71W28
   CHN: a-Baoji-Xinjie (Shaanxi; CRI 150 kW; CNR2 9820) "722" 34N30-107E10
        as-Baoji-Sifangshan (Shaanxi; CNR1,8) "724" 37N27-107E41
        b-Beijing-Matoucun "572" (100 kW CNR1) 39N45-116E49
        B-Beijing-Chaoyang/Gaobeidian/Shuangqiao "491" (CNR2-8) 39N53-116E34
        bd-Beijing-Doudian (150/500 kW CRI) "564" 39N38-116E05
        bm-Beijing BAF 39N54-116E28
        bs-Beijing 3SD 39N42-115E55
        c-Chengdu (Sichuan) 30N54-104E07
        cc-Changchun "523" (Jilin) 44N01'44"-125E25'08"
        ch-Changzhou Henglinchen "623" (Jiangsu) 31N42'33"-120E06'44"
        d-Dongfang (Hainan) 18N53-108E39
        da-Dalian 38N55-121E39
        db-Dongfang-Basuo 19N06-108E37
        e-Gejiu (Yunnan) 23N21-103E08
        eb-Beijing, Posolstvo 39N55-116E27
        f-Fuzhou (Fujian) 26N06-119E24
        fz-Fuzhou XSL (Fujian) 26N03-119E18
        g-Gannan (Hezuo) 34N58'30"-102E55
        gu-Gutian-Xincheng 26N34-118E44
        gx-Guangzhou XSQ 23N09-113E29
        gy-Guiyang 26N25-106E36
        gz-Guangzhou-Huadu (Guangdong) 23N24-113E14
        h-Hohhot "694" (Nei Menggu, CRI) 40N48-111E47
        ha-Hailar (Nei Menggu) 49N11-119E43
        hd-Huadian "763" (Jilin) 43N07-126E31
        he-Hezuo 34N58'14"-102E54'32"
        hh-Hohhot-Yijianfang (Nei Menggu, PBS NM) 40N43-111E33
        hk-Haikou (Hainan) 20N01-110E17
        hu-Hutubi (Xinjiang) 44N10-86E54
        j-Jinhua 29N07-119E19
        k-Kunming-Anning CRI (Yunnan) 24N53-102E30
        ka-Kashi (Kashgar) (Xinjiang) 39N21-75E46
        kl-Kunming-Lantao PBS (Yunnan) 25N10-102E50
        L-Lingshi "725" (Shanxi) 36N52-111E40
        n-Nanning (Guangxi) "954" 22N47-108E11
        nj-Nanjing (Jiangsu) 32N02-118E44
        nm-Nei Menggu network, see http://www.asiawaves.net/ for details
        p-Pucheng (Shaanxi) 35N00-109E31
        pt-Putian (Fujian) 25N28-119E10
        q-Ge'ermu/Golmud "916" (Qinghai) 36N26-95E00
        qq-Qiqihar 47N02-124E03 (500kW)
        qz-Quanzhou "641" (Fujian) 24N53-118E48
        s-Shijiazhuang "723" (Hebei; Nanpozhuang CRI 500 kW; Huikou CNR 100 kW) 38N13-114E06
        sg-Shanghai-Taopuzhen 31N15-121E29
        sh-Shanghai XSG 31N06-121E32
        sn-Sanya (Hainan) 18N14-109E19
        sq-Shangqiu (Henan) 34N56'54"-109E32'34"
        sw-Nanping-Shaowu (Fujian) 27N05-117E17
        sy-Shuangyashan "128" (Heilongjiang) 46N43'19"-131E12'40"
        t-Tibet (Lhasa-Baiding "602") 29N39-91E15
        tj-Tianjin 39N03'00"-117E25'30"
        u-Urumqi (Xinjiang, CRI) 44N08'47"-86E53'43"
        uc-Urumqi-Changji (Xinjiang, PBS XJ) 43N58'26"-87E14'56"
        x-Xian-Xianyang "594" (Shaanxi) 34N12-108E54
        xc-Xichang (Sichuan) 27N49-102E14
        xg-Xining (Qinghai) 36N39-101E35
        xm-Xiamen (Fujian) 24N29'32"-118E04'23"
        xt-Xiangtan (Hunan) 27N30-112E30
        xw-Xuanwei (Yunnan) 26N09-104E02
        xx-Unknown site
        xy-Xingyang (Henan) 34N48-113E23
        xz-Xinzhaicun (Fujian) 25N45-117E11
        ya-Yanbian-Yanji (Jilin) 42N47'30"-129E29'18"
        zh-Zhuhai "909" (Guangdong) 22N23-113E33
        zj-Zhanjiang (Guangdong) 21N11-110E24
        zs-Mount Putuo, Xiaohulu Island, Zhoushan 30N00-122E23
   CKH: rt-Rarotonga 21S12-159W49
   CLM: b-Barranquilla 10N55-074W46
        r-Rioblanco, Tolima 03N30-075W50
   CLN: e-Ekala (SLBC,RJ) 07N06-79E54
        i-Iranawila (IBB) 07N31-79E48
        p-Puttalam 07N59-79E48
        t-Trincomalee (DW) 08N44-81E10
   CME: Buea 04N09-09E14
   CNR: ar-Arrecife (Lanzarote) 29N08-13W31
        fc-Fuencaliente (Las Palmas)
        gc-Gran Canaria airport 27N57-15W23
        hr-Haría (Tenerife)
        lm-Las Mesas (Las Palmas) 28N06-15W28
   COD: bk-Bukavu 02S30-28E50
        bu-Bunia 01N32-30E11
   COG: b-Brazzaville-M'Pila 04S15-15E18
        pn-Pointe Noire 04S47-11E52
   CTI: a-Abidjan 05N22-03W58
   CTR: Cariari de Pococí (REE) 10N25-83W43 except:
        g-Guápiles (Canton de Pococí, Prov.de Limón) ELCOR 10N13-83W47
   CUB: La Habana sites Quivicán/Bejucal/Bauta 23N00-82W30
        b-Bauta 22N57-82W33
        be-Bejucal
        hr-Havana Radio 23N10-82W19
        q-Quivicán/Titan 22N50-82W18
   CVA: Santa Maria di Galeria 42N03-12E19 except:
        v-Citta del Vaticano 41N54-12E27
   CYP: a-Akrotiri (UK territory) 34N37-32E56
        cr-Cyprus Radio 35N03-33E17
        g-Cape Greco 34N57-34E05
        m-Lady's Mile (UK territory) 34N37-33E00
        n-Nicosia 35N10-33E21
        y-Yeni Iskele 35N17-33E55
   CZE: b-Brno-Dobrochov 49N23-17E08
        cb-Ceske Budejovice-Husova kolonie 48N59'34"-14E29'37"
        dl-Dlouhá Louka 50N38'53"-13E39'22"
        kv-Karlovy Vary-Stará Role 50N14'22"-12E49'26"
        mb-Moravské Budejovice-Domamil 49N04'35"-15E42'24"
        os-Ostrava-Svinov 49N48'41"-18E11'36"
        p-Praha 50N02-14E29
        pl-Praha-Liblice 50N04'43"-14E53'13"
        pr-Pruhonice / Pruhonice 49N59'28"-14E32'17"
        pv-Panská Ves 50N31'41"-14E34'01"
        tr-Trebon / Trebon 49N00-14E46
        va-Vackov 50N14-12E23
   D:   al-Albersloh 51N53'11"-07E43'19"
        b-Biblis 49N41'18"-08E29'32"
        be-Berlin-Britz 52N27-13E26
        bl-Berlin 52N31-13E23
        br-Braunschweig 52N17-10E43
        bu-Burg 52N17'13"-11E53'49"
        bv-Bonn-Venusberg 50N42'29"-07E05'48"
        d-Dillberg 49N19-11E23
        dd-Dresden-Wilsdruff 51N03'31"-13E30'27"
        dt-Datteln 51N39-07E21
        e-Erlangen-Tennenlohe 49N35-11E00
        fl-Flensburg 54N47'30"-09E30'12"
        g-Goehren 53N32'08"-11E36'40"
        ge-Gera 50N53-12E05
        gl-Glücksburg 54N50-09E30 and Neuharlingersiel, Schortens, Hürup, Rostock, Marlow
        ha-Hannover 52N23-09E42
        hc-Hamburg-Curslack 53N27-10E13
        he-Hannover/Hemmingen 52N19'40"-09E44'12"
        hh-Hamburg-Moorfleet 53N31'09"-10E06'10"
        ht-Hartenstein (Sachsen) 50N40-12E40
        jr-Juliusruh 54N37'45"-13E22'26"
        k-Kall-Krekel 50N28'41"-06E31'23"
        L-Lampertheim 49N36'17"-08E32'20"
        la-Langenberg 51N21'22"-07E08'03"
        li-Lingen 52N32'06"-07E21'11"
        mf-Mainflingen 50N00'56"-009E00'43"
        n-Nauen 52N38'55"-12E54'33"
        nh-Neuharlingersiel DHJ59 53N40'35"-07E36'45"
        nu-Nuernberg 49N27-11E05
        or-Oranienburg 52N47-13E23
        pi-Pinneberg 53N40'23"-09E48'30"
        r-Rohrbach 48N36-11E33
        rf-Rhauderfehn 53N05-07E37
        s-Stade 53N36-09E28
        w-Wertachtal 48N05'13"-10E41'42"
        wa-Winsen (Aller) 52N40-09E46
        wb-Wachenbrunn 50N29'08"-10E33'30"
        we-Weenermoor 53N12-07E19
        xx-Secret pirate site
   DJI: d-Djibouti 11N30-43E00
        i-Centre de Transmissions Interarmées FUV 11N32'09"-43E09'20"
   DNK: a-Aarhus-Mårslet 56N09-10E13
        bl-Blaavand 55N33-08E06
        br-Bramming 55N28-08E42
        bv-Bovbjerg 56N31-08E10
        co-Copenhagen OXT 55N50-11E25
        f-Frederikshavn 57N26-10E32
        h-Hillerod 55N54-012E16
        hv-Copenhagen Hvidovre 55N39-12E29
        i-Copenhagen Ishøj 55N37-12E21
        k-Kalundborg 55N40'35"-11E04'10"
        ra-Randers 56N28-10E02
        ro-Ronne 55N02-15E06
        sg-Skagen 57N44-10E34
        sk-Skamlebaek 55N50-11E25
   DOM: sd-Santo Domingo 18N28-69W53
   E:   af-Alfabia 39N44-02E43
        ag-Aguilas 37N29-01W34
        ar-Ares/Cabo Machichaco (Matxitxako) 43N27-02W45
        as-La Asomada 37N38-00W58
        bo-Boal
        c-Coruna 43N22'01"-08W27'07"
        cg-Cabo de Gata 36N43-02W12
        cp-Chipiona 36N40-06W24
        fi-Finisterre 42N54-09W16
        gm-Torreta de Guardamar, Guardamar del Segura 38N04'18"-00W39'53"
        h-Huelva 37N12-07W01
        jq-Jaizquibel 43N22-01W50
        ma-Madrid 40N28-03W40
        mu-Muxía 43N06-09W13
        mx-Marratxí 39N38-02E40
        n-Noblejas 39N57-03W26
        pm-Palma de Mallorca 39N22-02E47
        rq-Roquetas 36N44-02W38
        rs-Rostrío/Cabo de Peñas 43N39-05W51
        sb-Sabiner 36N41-02W42
        ta-Tarifa 36N03-05W33
        tj-Trijueque 40N47-02W59
        to-Torrejón de Ardoz (Pegaso, Pavon, Brujo) 40N30-03W27
           Jota,Polar-Zaragoza; Perseo-Alcala de los Gazules; Matador-Villatobas; Embargo-Soller
        vj-Vejer 36N16-06W01
   EGY: a-Abis 31N10-30E05
        al-Alexandria / Al-Iskandaria 31N12-29E54
        ca-Cairo 30N04-31E13
        ea-El Arish 31N06'43"-33E41'59"
        z-Abu Zaabal 30N16-31E22
   EQA: a-Ambato 01S13-78W37
        c-Pico Pichincha 00S11-78W32
        g-Guayaquil 02S16-79W54
        i-Ibarra 00N21-78W08
        o-Otavalo 00N18-78W11
        p-Pifo 00S14-78W20
        s-Saraguro 03S42-79W18
        t-Tena 01S00-77W48
        u-Sucúa 02S28-78W10
   ERI: Asmara-Saladaro 15N13-38E52
   EST: ta-Tallinn Radio 59N27-24E45
        tt-Tartu 58N25-27E06
        tv-Tallinn Volmet 59N25-24E50
   ETH: a-Addis Abeba 09N00-38E45
        ad-Adama 08N32-39E16
        b-Bahir Dar 11N36-37E23
        d-Geja Dera (HS) 08N46-38E40
        j-Geja Jewe (FS) 08N43-38E38
        jj-Jijiga 09N21-42E48
        m-Mekele 13N29-39E29
        n-Nekemte 09N05-36E33
        r-Robe 07N07-40E00
   F:   Issoudun 46N56-01E54 except:
        a-Allouis 47N10-02E12
        av-Avord 47N03-02E28
        br-Brest 48N25'33"-04W14'27"
        cm-Col de la Madone 43N47-07E25
        co-Corsen 48N25-04W48
        g-La Garde 43N06'16"-05E59'29"
        gn-Gris-Nez 50N52-01E35
        jb-Jobourg 49N41'05"-01W54'28"
        ma-Mont Angel/Fontbonne 43N46-07E25'30"
        p-Paris 48N52-02E18
        r-Rennes 48N06-01W41
        ro-Roumoules 43N47-06E09
        sa-Saissac (11) 43N23'25"-02E05'59"
        sb-Strasbourg 48N14'59"-07E26'37"
        sg-Saint Guénolé 47N49-04W23
        to-Toulon 43N08'08"-06E03'35"
        v-Favières FAV 48N32-01E14
        ve-Vernon 49N05-01E30
        wu-Rosnay (HWU) 46N43-01E14
   FIN: ha-Hailuoto (Oulu) 65N00-24E44
        he-Helsinki 60N09-24E44
        hk-Hämeenkyrö
        mh-Mariehamn (Aland Islands) 60N06-19E56
        o-Ovaniemi 60N10'49"-24E49'35"
        p-Pori 61N28-21E35
        t-Topeno, Loppi, near Riihimäki 60N46-24E17
        v-Virrat 62N23-23E37
        va-Vaasa 63N05-21E36
   FJI: n-Nadi-Enamanu 17S47'14"-177E25'20"
   FRO: t-Tórshavn 62N01-06W47
   FSM: Pohnpei 06N58-158E12
   G:   ab-Aberdeen (Gregness) 57N07'39"-02W03'13"
        an-Anthorn 54N54'40"-03W16'50"
        cf-Collafirth (Shetland) 60N32-01W23
        cm-Crimond (Aberdeenshire) 57N37-01W53
        cp-London-Crystal Palace 51N25-00W05
        cr-London-Croydon 51N25-00W05
        ct-Croughton (Northants) 51N59'50"-01W12'33"
        d-Droitwich 52N18-02W06
        ev-St.Eval (Cornwall) 50N29-05W00
        fm-Falmouth (Lizard) 49N57'37"-05W12'06"
        hu-Humber (Flamborough) 54N07'00"-00W04'41"
        ic-one of: Inskip (Lancashire) 53N50-02W50 / St.Eval (Cornwall) 50N29-05W00 / Crimond (Aberdeenshire) 57N37-01W53
        in-Inskip (Lancashire) 53N50-02W50
        lo-London 51N30-00W11
        nw-Northwood 51N30-00W10
        o-Orfordness 52N06-01E35
        p-Portland 50N36-02W27
        r-Rampisham 50N48'30"-02W38'35"
        s-Skelton 54N44-02W54
        sc-Shetland (Lerwick) 60N24'06"-01W13'27"
        sp-Saint Peter Port (Guernsey) 49N27-02W32
        st-Stornoway (Butt of Lewis) 58N30'55"-06W15'40"
        ty-Tyree (Clyde) 56N30-06W59
        w-Woofferton 52N19-02W43
        wa-Washford (Somerset) 51N09'38"-03W20'55"
   GAB: Moyabi 01S40-13E31
   GEO: s-Sukhumi 43N00-41E00
   GNE: b-Bata 01N46-09E46
        m-Malabo 03N45-08E47
   GRC: a-Avlis 38N23-23E36
        i-Iraklion 35N20-25E07
        k-Kerkyra 39N37-19E55
        L-Limnos (Myrina) 39N52-25E04
        o-Olimpia 37N36'27"-21E29'15"
        r-Rhodos 36N25-28E13
   GRL: aa-Aasiaat 68N41-52W50
        ik-Ikerasassuaq (Prins Christian Sund) 60N00-44W00
        ko-Kook Island 64N04-52W01
        ma-Maniitsoq 65N25-52W54
        n-Nuuk 64N10-51W44
        pa-Paamiut 62N00-49W40
        qa-Qaqortoq 60N43-46W02
        qe-Qeqertarsuaq 69N15-53W33
        si-Sisimiut 66N56-53W40
        sq-Simiutaq 60N41-46W36
        t-Tasiilaq/Ammassalik 65N36-37W38
        up-Upernavik 72N47-56W09
        uu-Uummannaq 70N40-52W08
   GUF: Montsinery 04N54-52W29
   GUI: c-Conakry-Sonfonia 09N41'10"-13W32'11"
   GUM: a-Station KSDA, Agat, 13N20'28"-144E38'56"
        an-Andersen Air Force Base 13N34-144E55
        b-Barrigada 13N29-144E50
        h-Agana HFDL site 13N28-144E48
        m-Station KTWR, Agana/Merizo 13N16'38"-144E40'16"
        n-Naval station NPN 13N26-144E39
   GUY: Sparendaam 06N49-58W05
   HKG: a-Hongkong Airport 22N18-113E55
        m-Marine Rescue Radio VRC 22N17'24"-114E09'12"
   HND: t-Tegucigalpa 14N04-87W13
   HNG: b-Budapest 47N30-19E03
        g-Györ
        lh-Lakihegy 47N22-19E00
        m-Marcali-Somogyszentpal
        p-Pecs-Kozarmisleny
        sz-Szolnok-Besenyszoegi ut
   HOL: a-Alphen aan den Rijn 52N08-04E38
        b-Borculo 52N07-06E31
        cg-Coast Guard Den Helder - Scheveningen 52N06-04E15
        e-Elburg 52N26-05E52
        he-Heerde 52N23-06E02
        k-Klazienaveen 52N44-06E59
        m-Margraten 50N48-05E48
        n-Nijmegen 51N51-05E50
        o-Ouddorp, Goeree-Overflakkee island 51N48-03E54
        ov-Overslag 51N12-03E52
        xx-Secret pirate site
        zw-Zwolle 52N31-06E05
   HRV: Deanovec 45N39-16E27
   HWA: a-WWVH 21N59'21"-159W45'52"
        b-WWVH 21N59'11"-159W45'45"
        c-WWVH 21N59'18"-159W45'51"
        d-WWVH 21N59'15"-159W45'50"
        hi-Hickam AFB 21N19-157W55
        ho-Honolulu/Iroquois Point 21N19'32"-157W59'31"
        L-Lualualei 21N25'12"-158W08'54"W
        m-Moloka'i 21N11-157W11
        n-Naalehu 19N01-155W40
        nm-NMO Honolulu/Maili 21N25'41"-158W09'11"
        p-Pearl Harbour 21N25'41"-158W09'11"
   I:   a-Andrate 45N31-07E53
        an-Ancona IPA 43N36-13E28
        au-Augusta IQA 37N14-15E14
        b-San Benedetto de Tronto IQP 42N57-13E53
        ba-Bari IPB 41N05-16E59
        cg-Cagliari IDC 39N14-09E14
        cv-Civitavecchia IPD 42N02-11E50
        eu-Radio Europa, NW Italy
        ge-Genova ICB 44N26-08E56
        kr-Crotone IPC 39N03-17E08
        li-Livorno IPL 43N30-10E20
        lm-Lampedusa IQN 35N30-12E36
        me-Messina IDF 38N11-15E33
        mz-Mazara del Vallo IQQ 37N39-12E35
        na-Napoli IQH 40N51-14E14
        p-Padova 45N09-11E42
        pa-Palermo IPP 38N08-13E23
        pt-Porto Torres IZN 40N50-08E24
        r-Roma 41N48-12E31
        ra-Roma IMB 41N47-12E28
        re-Rome 41N55-12E29
        si-Sigonella (Sicilia) 37N24-14E55
        sr-Santa Rosa (Maritele), Roma 41N59-12E22
        sy-NSY 37N07-14E26
        t-Trieste IQX 45N40-13E46
        v-Viareggio, Toscana 43N54-10E17
        xx-Secret pirate site
   IND: a-Aligarh (4x250kW) 28N00-78E06
        ah-Ahmedabad 22N52-72E37
        az-Aizawl(10kW) 23N43-92E43
        b-Bengaluru-Doddaballapur (Bangalore) 13N14-77E30
        bh-Bhopal(50kW) 23N10-77E30
        c-Chennai (Madras) 13N08-80E07
        d-Delhi (Kingsway) 28N43-77E12
        dn-Delhi-Nangli Poona 28N46-77E08
        g-Gorakhpur 26N52-83E28
        gt-Gangtok 27N22-88E37
        hy-Hyderabad 17N20-78E33
        im-Imphal 24N37-93E54
        it-Itanagar 27N04-93E36
        j-Jalandhar 31N19-75E18
        ja-Jaipur 26N54-75E45
        je-Jeypore 18N55-82E34
        jm-Jammu 32N45-75E00
        k-Kham Pur, Delhi 110036 (Khampur) 28N49-77E08
        kc-Kolkata-Chandi 22N22-88E17
        kh-Kohima 25N39-94E06
        ko-Kolkata(Calcutta)-Chinsurah 23N01-88E21
        ku-Kurseong 26N55-88E19
        kv-Kolkata Volmet 22N39-88E27
        le-Leh 34N08-77E29
        lu-Lucknow 26N53-81E03
        m-Mumbai (Bombay) 19N11-72E49
        mv-Mumbai Volmet 19N06-72E52
        n-Nagpur, Maharashtra 20N54-78E59
        nj-Najibabad, Uttar Pradesh 29N38-78E23
        p-Panaji (Goa) 15N31-73E52
        pb-Port Blair-Brookshabad 11N37-92E45
        r-Rajkot 22N22-70E41
        ra-Ranchi 23N24-85E22
        sg-Shillong 25N26-91E49
        si-Siliguri 26N46-88E26
        sm-Shimla 31N06-77E09
        sr-Srinagar 34N00-74E50
        su-Suratgarh (Rajasthan) 29N18-73E55
        t-Tuticorin (Tamil Nadu) 08N49-78E05
        tv-Thiruvananthapuram(Trivendrum) 08N29-76E59
        v-Vijayanarayanam (Tamil Nadu) 08N23-77E45
        vs-Vishakapatnam (Andhra Pradesh) 17N43-83E18
        w-Guwahati (1x200kW, 1x50kW) 26N11-91E50
   INS: bi-Biak, Papua 01S00-135E30
        bu-Bukittinggi, Sumatera Barat 00S18-100E22
        bw-Belawan, Medan, Sumatera 03N47-98E41
        f-Fakfak, Irian Jaya Barat 02S55-132E17
        g-Gorontalo, Sulawesi 00N34-123E04
        j-Jakarta (Cimanggis) 06S12-106E51
        jm-Jambi, Sumatera 01S38-103E34
        kd-Kendari, Sulawesi Tenggara 03S58-122E30
        md-Manado, Sulawesi Utara 01N12-124E54
        mk-Manokwari, Irian Jaya Barat 00S48-134E00
        mr-Merauke, Papua 08S33-140E27
        ms-Makassar, Sulawesi Selatan 05S10-119E25
        n-Nabire, Papua 03S15-135E36
        p-Palangkaraya, Kalimantan Tengah 00S11-113E54
        pa-Palu, Sulawesi Tengah 00S36-129E36
        pd-Padang, Sumatera Barat 00S06-100E21
        pk-Pontianak 00S05-109E16
        se-Serui, Japen, Papua 01S48-136E26
        sm-Simalungun, Sumatera
        so-Sorong, Papua 00S52-131E25
        t-Ternate, Maluku Utara 00N48-127E23
        w-Wamena, Papua 04S06-138E56
   IRL: mh-Malin Head 55N22'19"-07W20'21"
        s-Shannon 52N42'32"-08W54'47"
        tr-Tralee, Co.Kerry 52N16-09W42
        v-Valentia 51N55-10W19
        xx-Secret pirate site
   IRN: a-Ahwaz 31N20-48E40
        b-Bandar-e Torkeman 36N54-54E04
        bb-Bonab 37N18-46E03
        c-Chah Bahar 25N29-60E32
        j-Jolfa 38N56-45E36
        k-Kamalabad 35N46-51E27
        ke-Kish Island 26N34-53E56
        ki-Kiashar 37N24-50E01
        m-Mashhad 36N15-59E33
        mh-Bandar-e Mahshahr 30N37-49E12
        q-Qasr Shirin 34N27-45E37
        s-Sirjan 29N27-55E41
        t-Tayebad 34N44-60E48
        te-Tehran 35N45-51E25
        z-Zahedan 29N28-60E53
        zb-Zabol 31N02-61E33
   IRQ: d-Salah al-Din (Saladin) 34N27-43E35
        s-Sulaimaniya 35N33-45E26
   ISL: f-Fjallabyggd 66N09-18W55
        g-Keflavik/Grindavik 64N01-22W34
        n-Neskaupstadur 65N08-13W44
        r-Reykjavik Aero/HFDL 64N05-21W51
        v-Vestmannaeyjar 63N27-20W16
   ISR: h-Haifa 32N49-35E00
        ii-Unknown 1224
        L-Lod (Galei Zahal) 31N58-34E52
        sy-She'ar-Yeshuv 33N12'55"-35E38'41"
        y-Yavne 31N54-34E45
   J:   ao-Aonoyama Signal Station, Utazu (Kagawa) 34N18-133E49
        as-Asahikawa AF JJU22 43N48-142E22
        ay-Ashiya AB JJZ59 33N53-130E39
        c-Chiba Nagara 35N28-140E13
        ct-Chitose AB, Hokkaido JJR20 42N48-141E40
        es-Esaki Signal Station (Osaka Bay), Awaji island (Hyogo) 34N35'54"-134E59'32"
        f-Chofu Campus, Tokyo 35N39-139E33
        fu-Fuchu AB JJT55 35N41-139E30
        gf-Gifu AB JJV67 35N24-136E52
        h-Mount Hagane 33N27'54"-130E10'32"
        hf-Hofu / Bofu AB JJX36 34N02-131E33
        hm-Hamamatsu AB JJV56 34N45-137E42
        hy-Hyakurigahara AB JJT33 36N11-140E25
        io-Imabari Ohama Vessel Station (Kurushima), Imabari (Ehime) 34N05'25"-132E59'16"
        ir-Iruma / Irumagawa AB JJT44 35N51-139E25
        it-Itoman, Okinawa JFE 26N09-127E40
        iw-Isewan Signal Station, Cape Irago, Tahara (Aichi) 34N35-137E01
        k-Kagoshima JMH 31N19-130E31
        kg-Kumagaya AB JJT66 36N10-139E19
        ki-Kisarazu AB JJT22 35N24-139E55
        kk-Komaki AB (Nagoya) JJV23 35N15-136E55
        km-Kume Shima / Kumejina, Okinawa JJU66 26N22-126E43
        kn-Kanmon Oseto Strait Signal Station, 33N55-130E56
        ko-Komatsu AB JJV90 36N24-136E24
        ks-Kasuga AB JJZ37 33N32-130E28
        ku-Kumamoto JJE20 32N50-130E51
        ky-Kyodo 36N11-139E51
        kz-Kannonzaki Signal Station, Yokosuka (Kanagawa) 35N15'22"-139E44'36"
        m-Miura 35N08'23"-139E38'32"
        mh-Miho AB, Yonago JJX25 35N30-133E14
        ms-Misawa AB JJS21 40N42-141E22
        mt-Matsushima AB JJS32 38N24-141E13
        mu-Muroto (Kochi, Shikoku) 33N17-134E09
        mz-Makurazaki (Kagoshima, Kyushu) 31N16-130E18
        n-Nemuro 43N18-145E34
        nh-Naha AB, Okinawa 26N12-127E39
        nk-Nagoya Kinjo Signal Station, Nagoya (Aichi) 35N02'06"-136E50'47"
        nr-Nara JJW24 34N34-135E46
        ny-Nyutabaru AB JJZ26 32N05-131E27
        o-Mount Otakadoya 37N22'22"-140E50'56"
        oe-Okinoerabu JJZ44 27N26-128E42
        ok-Okinawa 26N29-127E56
        os-Osaka JJD20 34N47-135E26
        ot-Otaru, Hokkaido JJS65 43N11-141E00
        sa-Sapporo / Chitose AB JJA20 42N47-141E40
        sn-Sendai / Kasuminome AB JJB20 38N14-140E55
        sz-Shizuoka 34N59-138E23
        tk-Tokyo / Tachikawa AF JJC20 JJT88 35N43-139E24
        ts-Tsuiki AB JJZ48 33N41-131E02
        tv-Tokyo Volmet, Kagoshima Broadcasting Station 31N43-130E44
        xx-Unknown site
        y-Yamata 36N10-139E50
        yo-Yokota AFB 35N44'55"-139E20'55"
        yz-Yozadake (Okinawa) 26N08-127E42
   JOR: ak-Al Karanah / Qast Kherane 31N44-36E26
        am-Amman 31N58-35E53
   KAZ: a-Almaty 43N15-76E55
        ak-Aktyubinsk/Aktöbe 50N15-57E13
        av-Almaty Volmet 43N21-77E03
        n-Nursultan (Akmolinsk/Tselinograd/Astana) 51N01-71E28
   KEN: ny-Nairobi 5YE 01S15-36E52
   KGZ: b-Bishkek (Krasnaya Rechka) 42N53-74E59
        bk- Bishkek Beta 43N04-73E39
   KOR: c-Chuncheon 37N56-127E46
        d-Dangjin 36N58-126E37
        db-Daebu-do (Ansan) 37N13'14"-126E33'29"
        g-Goyang / Koyang, Gyeonggi-do / Kyonggi-do 37N36-126E51
        h-Hwasung/Hwaseong 37N13-126E47
        j-Jeju/Aewol HLAZ 33N29-126E23
        k-Kimjae 35N50-126E50
        m-Muan HFDL 35N1'56"-126E14'19"
        n-Hwaseong? 37N13-126E47
        o-Suwon-Osan/Hwaseong-Jeongnam 37N09-127E00
        s-Seoul-Incheon HLKX 37N25-126E45
        sg-Seoul-Gangseo-gu 37N34-126E58
        t-Taedok 36N23-127E22
        w-Nowon Gyeonggi-do / Seoul-Taereung 37N38-127E07
        xx-Unknown site
   KOS: b-Camp Bondsteel 42N22-21E15
   KRE: c-Chongjin 41N47-129E50
        e-Hyesan 41N04-128E02
        h-Hamhung 39N56-127E39
        hw-Hwadae/Kimchaek 40N41-129E12
        j-Haeju 38N01-125E43
        k-Kanggye 40N58-126E36
        kn-Kangnam 38N54-125E39
        p-Pyongyang 39N05-125E23
        s-Sariwon 38N05-125E08
        sg-Samgo 38N02-126E32
        sn-Sinuiju 40N05-124E27
        sw-Sangwon 38N51-125E31
        u-Kujang 40N05-126E05
        w-Wonsan 39N05-127E25
        y-Pyongsong 40N05-124E24
   KWT: j-Jahra/Umm al-Rimam 29N30-47E40
        k-Kabd/Sulaibiyah 29N09-47E46
        kw-Kuwait 29N23-47E39
   LAO: s-Sam Neua 20N16-104E04
        v-Vientiane 17N58-102E33
   LBN: be-Beirut 33N51-35E33
   LBR: e-Monrovia ELWA 06N14-10W42
        m-Monrovia Mamba Point 06N19-10W49
        s-Star Radio Monrovia 06N18-10W47
   LBY: Sabrata 32N54-13E11
   LTU: Sitkunai 55N02'37"-23E48'28" except:
        v-Viesintos 55N42-24E59
   LUX: j-Junglinster 49N43-06E15
        m-Marnach 50N03-06E05
   LVA: Ulbroka 56N56-24E17
   MAU: m-Malherbes 20S20'28"-57E30'46"
   MDA: Maiac near Grigoriopol 47N17-29E24 except:
        ca-Cahul 45N56-28E17
        ce-Chisinau 47N01-28E49
        co-Codru-Costiujeni 46N57-28E50
        ed-Edinet 48N11-27E18
   MDG: Talata Volonondry 18S50-47E35 except:
        a-Ambohidrano/Sabotsy 18S55-47E32
        m-Mahajanga (WCBC) 15S43'38"-46E26'45"
   MDR: ps-Porto Santo 33N04-16W21
   MEX: c-Cuauhtémoc, Mexico City 19N26-99W09
        e-Mexico City (Radio Educación) 19N16-99W03
        i-Iztacalco, Mexico City 19N23-98W57
        m-Merida 20N58-89W36
        mz-Mazatlan
        pr-Progreso 21N16-89W47
        s-San Luis Potosi 22N10-101W00
        p-Chiapas 17N00-92W00
        u-UNAM, Mexico City 19N23-99W10
        vh-Villahermosa, Tabasco 18N00-93W00
   MLA: ka-Kajang 03N01-101E46
        kk-Kota Kinabalu 06N12-116E14
        ku-Kuching-Stapok (closed 2011) 01N33-110E20
        l-Lumut 04N14-100E38
        s-Sibu 02N18-111E49
   MLI: c-CRI-Bamako 12N41-08W02
        k-Kati(Bamako) 12N45-08W03
   MLT: mr-Malta Radio 35N49-14E32
   MNE: oc-Ocas 42N01-19E08
   MNG: a-Altay 46N19-096E15
        c-Choybalsan
        m-Moron/Mörön 49N37-100E10
        u-Ulaanbaatar-Khonkhor 47N55-107E00
   MRA: m-Marpi, Saipan (KFBS) 15N16-145E48
        s-Saipan/Agingan Point (IBB) 15N07-145E41
        t-Tinian (IBB) 15N03-145E36
   MRC: ag-Agadir 30N22-09W33
        b-Briech (VoA/RL/RFE) 35N33-05W58
        ca-Casablanca 33N37-07W38
        L-Laayoune (UN 6678) 27N09-13W13
        n-Nador (RTM,Medi1) 34N58-02W55
        s-Safi 32N18-09W14
   MRT: fc-Fort-de-France CROSS 14N36-61W05
        u-FUF Martinique 14N31'55"-60W58'44"
   MTN: Nouakchott 18N07-15W57
   MYA: n-Naypyidaw 20N11-96E08
        p-Phin Oo Lwin, Mandalay 22N01-96E33
        t-Taunggyi(Kalaw) 20N38-96E35
        y-Yegu (Yangon/Rangoon) 16N52-96E10
   NCL: n-Nouméa - Ouen-Toro 22S18'19"-166E27'17"
   NGR: Niamey 13N30-02E06
   NIG: a-Abuja-Gwagwalada 08N56-07E04
        b-Ibadan 07N23-03E54
        e-Enugu 06N27-07E27
        i-Ikorodu 06N36-03E30
        j-Abuja-Lugbe (new site, opened March 2012) 08N58-07E21
        k-Kaduna 10N31-07E25
   NMB: wb-Walvis Bay 23S05-14E38
   NOR: a-Andenes 69N16'08"-16E02'26"
        as-Andenes-Saura 69N08'24"-16E01'12"
        at-Andenes (Telenor site) 69N17-16E03
        be-Bergen (LLE station, Erdal, Askoy Island) 60N26-05E13
        bk-Bergen-Kvarren 60N23'22"-05E15
        bo-Bodø
        bs-Bodø-Seines 67N12-14E22
        bv-Berlevåg 70N52-29E04
        e-Erdal 60N26'56"-05E12'59"
        f-Florø 61N36-05E02
        fs-Farsund 58N05-06E47
        hf-Hammerfest 70N40-23E41
        jm-Jan Mayen Island 71N00-08W30
        ly-Longyearbyen, Svalbard 78N04-13E37
        ma-Marøy 60N42-04E53
        mg-Molde-Gossen 62N50'30"-06E47'35"
        mr-Mågerø 59N09-10E26
        nm-Nordmela-Andøya 69N07'40"-15E38
        no-Novik 66N58'58"-13E52'23"E
        oh-Oslo-Helgelandsmoen 60N07-10E12
        or-Ørlandet 63N41-09E39
        ro-Rogaland (Vigreskogen) 58N39-05E35
        sa-Sandnessjøen 66N01-12E38
        sr-Sørreisa 69N04-18E00
        st-Stavanger-Ulsnes 59N00-05E43
        tj-Tjøme
        va-Vardø 70N22-31E06
   NPL: Khumaltar 27N30-85E30
   NZL: a-Auckland (Wiroa Island) 37S01-174E49
        du-Dunedin 45S52-170E30
        r-Rangitaiki 38S50-176E25
        ru-Russell 35S17-174E07
        t-Taupo 38S52-176E26
   OCE: fa-Faa'a airport 17S33-149W37
        ma-Mahina (Tahiti) 17S30'21"-149W28'57"
   OMA: a-A'Seela 21N55-59E37
        s-Seeb 23N40-58E10
        t-Thumrait 17N38-53E56
   PAK: i-Islamabad 33N27-73E12
        kv-Karachi Volmet 24N54-67E10
        m-Multan 30N05'22"-71E29'30"
        p-Peshawar 34N00-71E30
        q-Quetta 30N15-67E00
        r-Rawalpindi 33N30-73E00
   PHL: b-Bocaue (FEBC) 14N48-120E55
        dv-Davao City, Mindanao 07N05-125E36
        i-Iba (FEBC) 15N20-119E58
        ko-Koronadal City, Mindanao 06N31-124E49
        m-Marulas/Quezon City, Valenzuela (PBS 6170,9581) 14N41-120E58
        p-Palauig, Zembales (RVA) 15N28-119E50
        po-Poro 16N26-120E17
        sc-Santiago City, Luzon 16N42-121E36
        t-Tinang (VoA) 15N21-120E37
        x-Tinang-2/portable 50kW (VoA) 15N21-120E37
        zm-Zamboanga City, Mindanao 06N55-122E07
   PLW: Koror-Babeldaob (Medorn) 07N27'22"-134E28'24"
   PNG: a-Alotau 10S18-150E28
        b-Bougainville/Buka-Kubu 05S25-154E40
        d-Daru 09S05-143E10
        g-Goroka 06S02-145E22
        ka-Kavieng 02S34-150E48
        kb-Kimbe 05S33-150E09
        ke-Kerema 07S59-145E46
        ki-Kiunga 06S07-141E18
        ku-Kundiawa 06S00-144E57
        la-Lae (Morobe) 06S44-147E00
        ln-Lae Nadzab airport 06S34-146E44
        lo-Lorengau 02S01-147E15
        ma-Madang 05S14-145E45
        me-Mendi 06S13-143E39
        mh-Mount Hagen 05S54-144E13
        pm-Port Moresby (Waigani) 09S28-147E11
        pr-Port Moresby Maritime Radio 09S28-147E11
        po-Popondetta 08S45-148E15
        r-Rabaul 04S13-152E13
        v-Vanimo 02S40-141E17
        va-Vanimo 02S41-141E18
        wa-Port Moresby (Radio Wantok) 09S28-147E11
        ww-Wewak 03S35-143E40
   PNR: al-Albrook, Panama City 08N58'08"-79W33'03"
   POL: p-Puchaly, in Falenty 52N08'37"-20E54
        sk-Solec Kujawski 53N01'13"-18E15'44"
        w-Witowo 54N33-16E32
   POR: ms-Monsanto 38N44-09W11
   PRG: c-Capiatá 25S24-57W28
        f-Filadelfia 22S21-60W02
   PRU: ar-Arequipa 16S25-71W32
        at-Atalaya 10S43'48"-73W45'20"
        bv-Bolívar 07S21-77W50
        cc-Chiclayo/Santa Ana (Carretera a Lambayeque) 06S44-79W51
        ce-Celendín 06S53-78W09
        ch-Chachapoyas 06S14-77W52
        cl-Callalli 15S30-71W26
        cp-Cerre de Pasco 10S40-76W15
        ct-Chota 06S33-78W39
        cu-Cuzco-Cerro Oscollo 13S31'08"-72W00'37"
        cz-Chazuta/Tarapoto, San Martin 06S34-76W08
        hb-Huancabamba 05S14-79W27
        hc-Huancayo/Viques 12S12'06"-75W13'11"
        ho-Huánuco 09S56-76W14
        ht-Huanta/Tirapampa 12S57-74W15
        hu-Huanta/Vista Alegre (Pasaje Amauta) 12S56'12"-74W15'10"
        hv-Huancavelica 12S47-74W59
        hz-Huaraz 09S31-77W32
        in-Chau Alto/Independencia, Huarez, Ancash (planned for 6090 kHz in 2015/16) 09S31'05"-77W32'57"
        iq-Iquitos/Moronacocha 03S45-73W16
        ja-Jaén 05S45-78W51
        ju-Junín/Cuncush 11S10-76W00
        li-Lima 12S06-77W03
        or-La Oroya 11S32-75W54
        pc-Paucartambo 10S54-75W51
        pm-Puerto Maldonado 12S36-69W10
        qb-Quillabamba/Macamango 12S52-72W42
        qt-Quillabamba/Tiobamba Baja 12S49-72W41
        rm-Rodrigues de Mendoza 06S23-77W30
        sc-Santa Cruz (R Satelite) 06S41-79W02
        si-Sicuani 14S16-71W14
        su-Santiago de Chuco 08S09-78W11
        ta-Tarma/Cerro Penitencia 11S24'32"-75W41'31"
        tc-Tacna 18S00-70W13
        ur-valle de Urubamba, Cusco 13S21-72W07
        yu-Yurimaguas 05S54-76W07
   PTR: i-Isabela 18N23-67W11
        s-Salinas, Camp Santiago 17N59-66E18
   QAT: dr-Doha Radio 25N42-16E32
   REU: su-Sainte-Suzanne 20S54'36"-55E35'05"
   ROU: b-Bucuresti/Bucharest airport 44N34-26E05
        c-Constanta 44N06-28E37
        g-Galbeni 46N45-26E41
        s-Saftica 100kW 44N38-27E05
        t-Tiganesti 300kW 44N45-26E05
   RRW: Kigali 01S55-30E07
   RUS: a-Armavir/Tblisskaya/Krasnodar 45N29-40E07
        ak-Arkhangelsk Beta 64N24-41E32
        am-Amderma 69N46-61E34
        an-Astrakhan Narimanovo 46N17-48E00
        ar-Arkhangelsk 64N22-41E24
        as-Astrakhan Military Base 47N25-47E55
        at-Arkhangelsk-Talagi 64N36-40E43
        b-Blagoveshchensk (Amur) 50N16-127E33
        bo-Bolotnoye, Novosibirsk oblast 55N45'22"-84E26'52"
        B1-Buzzer sites: Kerro (St Petersburg) 60N18'40"-30E16'40"
        B2-Buzzer sites: Naro-Fominsk (Moscow) 55N25'35"-36E42'33"
        c-Chita (Atamanovka) (S Siberia) 51N50-113E43
        cb-Cheboksary
        cy-Chelyabinsk 55N18-61E30
        ek-Yekaterinburg
        el-Elista 46N22-44E20
        gr-Grozny 43N16-45E43
        i-Irkutsk (Angarsk) (S Siberia) 52N25-103E40
        io-Ioshkar-Ola
        iv-Irkutsk Volmet 52N16-104E23
        iz-Izhevsk sites 56N50-53E15
        k-Kaliningrad-Bolshakovo 54N54-21E43
        ka-Komsomolsk-na-Amur (Far East) 50N39-136E55
        kd-Krasnodar Beta 44N36-39E34
        kf-Krasnoyarsk HFDL site 56N06-92E18
        kg-Kaliningrad Radio UIW23 54N43-20E44
        kh-Khabarovsk (Far East) 48N33-135E15
        kl-Kaliningrad Military Base 54N39-19E55
        km-Khanty-Mansiysk 61N02-69E05
        kp-Krasnodar-Poltovskaya 45N24'18"-38E09'29"
        kr-Krasnoyarsk 56N02-92E44
        kt-Kotlas 61N14-46E42
        ku-Kurovskaya-Avsyunino (near Moscow) 55N34-39E09
        kv-Kirensk Volmet 57N46-108E04
        ky-Kyzyl 51N41-94E36
        kz-Kazan 55N36-49E17
        k1-Krasnodar Pashkovsky 45N02-39E10
        L-Lesnoy (near Moscow) 56N04-37E58
        m-Moscow/Moskva (one of ku,L,se,t) 55N45-37E18 
        ma-Magadan/Arman 59N42-150E10
        mg-Magadan Military Base 59N42-150E10
        mi-Mineralnye Vody 44N14-43E05
        mk-Makhachkala, Dagestan 42N49-47E39
        mm-Murmansk Meteo 69N00-33E00
        mp-Maykop 44N40-40E02
        mr-Moscow-Razdory 55N45-37E18
        mt-MTUSI University, Moscow 55N45'15"-37E42'43"
        mu-Murmansk/Monchegorsk 67N55-32E59
        mv-Magadan Volmet 59N55-150E43
        mz-Mozdok, North Ossetia 43N45-44E39
        n-Novosibirsk / Oyash, (500 kW, 1000 kW) 55N31-83E45
        nc-Nalchik, Kabardino-Balkaria 43N31-43E38
        ni-Nizhnevartovsk 60N57-76E29
        nk-Nizhnekamsk
        nm-Naryan-Mar 67N38-53E07
        nn-Nizhni Novgorod sites 56N11-43E58
        no-Novosibirsk City (100 kW, 200 kW) 55N04-82E58
        nu-Novy Urengoy 66N04-76E31
        nv-Novosibirsk sites 55N01-82E40
        ny-Nadym 65N29-72E42
        og-Orenburg-Gagarin airport 51N48-55E27
        om-Omsk 54N58-73E19
        or-Orenburg 51N46-55E06
        p-Petropavlovsk-Kamchatskij (Yelizovo) 53N11-158E24
        pc-Pechora 65N07-57E08
        pe-Perm 58N03-56E14
        pk-Petropavlovsk-Kamchatskij Military Base 53N10-158E27
        pm-Perm airport 57N55-56E01
        pt-Sankt Peterburg Military Base 60N00-30E00
        pu-Puteyets, Pechora, Rep.Komi 65N10-57E05
        pv-Sankt Peterburg Volmet / Pulkovo 59N48-30E15
        re-Revda 68N02'08"-34E31'00"
        ro-Rossosh, Voronezhskaya oblast 50N12-39E35
        rp-Rostov-na-Donu 47N17'58"-39E40'25"
        rv-Rostov Volmet, Rostov-na-Donu 47N30-39E55
        s-Samara (Zhygulevsk) 53N17-50E15
        sa-Samara Centre 53N12-50E10
        sd-Severodvinsk 64N34-39E51
        se-Sevastopol 44N56-34E28
        sh-Salekhard 66N35-66E36
        sk-Smolensk 54N47-32E03
        sm-Severomorsk/Arkhangelsk Military Base 69N03-33E19
        so-Sochi 43N27-39E57
        sp-St.Petersburg (Popovka/Krasnyj Bor) 59N39-30E42
        sr-Sevastopol Radio 44N34-33E32
        st-Saratov 51N32-45E51
        su-Surgut 61N20-73E24
        sv-Samara airport 53N30-50E09
        sy-Syktyvkar Volmet 61N39-50E51
        sz-Syzran
        s1-Saratov airport 51N34-46E03
        s2-Stavropol airport 45N07-42E07
        t-Taldom - Severnyj, Radiotsentr 3 (near Moscow) 56N44-37E38
        to-Togliatti
        ts-Tarko-Sale 64N56-77E49
        tv-Tavrichanka (Vladivostok, 549, 1377) 43N20-131E54
        ty-Tyumen Volmet 57N10-65E19
        u-Ulan-Ude 51N44-107E26
        uf-Ufa 54N34-55E53
        us-Ulan-Ude/Selenginsk 52N02-106E56
        v-Vladivostok Razdolnoye (Ussuriysk) 43N32-131E57
        vg-Vologda 59N17-39E57
        vk-Vorkuta 67N29-63E59
        vl-Vladivostok Military Base 43N07-131E54
        vo-Volgograd 48N47-44E21
        vz-Vladikavkaz Beslan, North Ossetia 43N12-44E36
        xe-Khabarovsk-Elban 50N04'24"-136E36'24"
        xv-Khabarovsk Volmet 48N31-135E10
        xx-Unknown site(s)
        xy-Ship position given as "99123 10456" meaning 12.3N-45.6E
        ya-Yakutsk/Tulagino 62N01-129E48
        ys-Yuzhno-Sakhalinsk (Vestochka) 46N55-142E54
        yv-Yekaterinburg Volmet (Koltsovo) 56N45-60E48
        yy-Yakutsk Volmet 62N05-129E46
   S:   b-Bjuröklubb 64N28-21E35
        d-Delsbo 61N48-16E33
        gr-Grimeton (Göteborg) 57N40-11E50
        gs-Gislövshammar 55N29-14E19
        h-Härnösand 62N38-17E56
        j-Julita 59N08-16E03
        k-Kvarnberget-Vallentuna 59N30-18E08
        s-Sala 59N36'25"-17E12'44"
        st-Stavsnäs 59N17-18E41
        t-Tingstäde 57N44-18E37
        v-Vaxholm, The Castle 59N24-18E21
        vb-Varberg 57N07'28"-12E14'09"
   SDN: a-Al-Aitahab 15N35-32E26
        r-Reiba 13N33-33E31
   SEN: r-Rufisque DIRISI 14N45'37"-17W16'26"
        y-Dakar Yoff 14N44-17W29
   SEY: mh-Mahe 04S37-55E26
   SLM: Honiara 09S26-160E03
   SNG: Kranji 01N25-103E44 except:
        j-Jurong 01N16-103E40
        v-Singapore Volmet 01N20-103E59
   SOM: b-Baydhabo 03N07-43E39
        g-Garoowe 08N24-48E29
        h-Hargeisa 09N33-44E03
        ma-Mogadishu Airport 02N01-45E18
   SRB: be-Beograd/Belgrade 44N48-20E28
        s-Stubline 44N34-20E09
   SSD: n-Narus 04N46-33E35
   STP: Pinheira 00N18-06E42
   SUI: be-Bern Radio HEB, Prangins 46N24-06E15
        ge-Geneva 46N14-06E08
        lu-Luzern (approx; Ampegon?) 46N50-08E24
   SUR: pm-Paramaribo 05N51-55W09
   SVK: Rimavska Sobota 48N24-20E08
   SWZ: Manzini/Mpangela Ranch 26S34-31E59
   SYR: Adra 33N27-36E30
   TCD: N'Djamena-Gredia 12N08-15E03
   THA: b-Bangkok / Prathum Thani 14N03-100E43
        bm-Bangkok Meteo 13N44-100E30
        bv-Bangkok Volmet 13N41'40"-100E46'14"
        hy-Hat Yai 06N56'11"-100E23'18"
        n-Nakhon Sawan 15N49-100E04
        u-Udon Thani 17N25-102E48
   TJK: da-Dushanbe airport 38N32-68E49
        y-Yangi Yul (Dushanbe) 38N29-68E48
        o-Orzu 37N32-68E42
   TKM: a-Asgabat 37N51-58E22
        as-Ashgabat airport 37N59-58E22
        ds-Dasoguz/Dashoguz 41N46-59E50
        s-Seyda/Seidi 39N28'16"-62E43'07"
        tb-Turkmenbashi 40N03-53E00
   TRD: np-North Post 10N45-61W34
   TUN: bz-Bizerte 37N17-09E53
        gu-La Goulette 36N49-10E18
        ke-Kelibia 36N50-11E05
        mh-Mahdia 35N31-11E04
        s-Sfax 34N48-10E53
        sf-Sfax 34N44-10E44
        tb-Tabarka 36N57-08E45
        te-Tunis 36N50-10E11
        tu-Tunis 36N54-10E11
        zz-Zarzis 33N30-11E06
   TUR: a-Ankara 39N55-32E51
        c-Cakirlar 39N58-32E40
        e-Emirler 39N29-32E51
        is-Istanbul TAH 40N59-28E49
        iz-Izmir 38N25-27E09
        m-Mersin 36N48-34E38
   TWN: f-Fangliao FAN 22N23-120E34
        h-Huwei (Yunlin province) 23N43-120E25
        k-Kouhu (Yunlin province) 23N32-120E10
        L-Lukang 24N03-120E25
        m-Minhsiung (Chiayi province) 23N34-120E25
        n-Tainan/Annan (Tainan city) 23N02-120E10
        p-Paochung/Baujong (Yunlin province) PAO/BAJ 23N43-120E18
        pe-Penghu (Pescadores), Jiangmei 23N38-119E36
        q-Tainan-Qigu/Cigu, Mount Wufen (Central Weather Bureau)
        s-Danshui/Tanshui/Tamsui (Taipei province) 25N11-121E25
        t-Taipei (Pali) 25N06-121E26
        w-Taipei, Mount Wufen (Central Weather Bureau) 25N09-121E34
        y-Kuanyin (Han Sheng) 25N02-121E06
        yl-Yilin 24N45-121E44
   TZA: d-Daressalam 06S50-39E14
        z-Zanzibar/Dole 06S05-39E14
   UAE: Dhabbaya 24N11-54E14
   UGA: k-Kampala-Bugolobi 00N20-32E36
        m-Mukono 00N21-32E46
   UKR: be-Berdiansk 46N38-36E46
        c-Chernivtsi 48N18-25E50
        k-Kiev/Brovary 50N31-30E46
        ke-Kiev 50N26-30E32
        L-Lviv (Krasne) (Russian name: Lvov) 49N51-24E40
        m-Mykolaiv (Kopani) (Russian name:Nikolayev) 46N49-32E14
        od-Odessa 46N29-30E44
        pe-Petrivka 46N54-30E43
        rv-Rivne 50N37-26E15
        x-Kharkiv (Taranivka) (Russian name: Kharkov) 49N38-36E07
        xx-Unknown site(s)
        z-Zaporizhzhya 47N50-35E08
   URG: lp-La Paloma 34S39-54W08
        m-Montevideo 34S50-56W15
        pc-Punta Carretas 34S48-56W21
        pe-Punta del Este 34S58-54W51
        rb-Rio Branco 32S34-53W23
        t-Tacuarembó 31S38-55W58
        tr-Trouville 34S52-56W18
   USA: a-Andrews AFB, MD 38N48'39"-76W52'01"
        b-Birmingham / Vandiver, AL (WEWN) 33N30'13"-86W28'27"
        ba-WBMD Baltimore, MD 39N19'26"-76W32'56"
        bg-Barnegat, NJ 39N45-74W23'30"
        bo-Boston, MA 41N42'30"-70W33'00"
        bt-Bethel, PA (WMLK) 40N28'46"-76W16'47"
        c-Cypress Creek, SC (WHRI) 32N41'03"-81W07'50"
        ch-Chesapeake - Pungo Airfield, VA 36N40'40"-76W01'40"
        cu-Cutler, ME 44N38-67W17
        da-Davidsonville, MD 
        ds-Destin, FL 30N23-86W26
        dv-Dover, NC (KNC) 35N13'01"-77W26'18"
        dx-Dixon, CA 38N22'46"-121W45'50"
        ej-Ellijay, GA (KJM) 34N38'08"-84W27'44"
        fa-Fort Collins, CO 40N40'55"-105W02'31"
        fb-Fort Collins, CO 40N40'42"-105W02'25"
        fc-Fort Collins, CO 40N40'48"-105W02'25"
        fd-Fort Collins, CO 40N40'45"-105W02'25"
        fe-Fort Collins, CO 40N40'53"-105W02'29"
        ff-Fort Collins, CO 40N40'41"-105W02'49"
        fg-Fort Collins, CO 40N40'51"-105W02'33"
        fv-Forest, VA 37N23'42"-79W12'35"
        g-Greenville, NC 35N28-77W12
        hw-KWHW Altus, OK 34N37'35"-99W20'10"
        jc-Jim Creek, WA 48N12-121W55
        k-Key Saddlebunch, FL 24N39-81W36
        L-Lebanon, TN (WTWW) 36N16'35"-86W05'58"
        LL-Lakeland, FL (WCY) 27N58'53"-082W03'05"
        lm-Lamoure, ND 46N22-98W20
        m-Miami / Hialeah Gardens, FL (WRMI) 25N54'00"-80W21'49"
        ma-Manchester / Morrison, TN (WWRB) 35N37'27'-86W00'52"
        mi-Milton, FL (WJHR) 30N39'03"-87W05'27"
        mo-Mobile, AL (WLO) 30N35'42"-88W13'17"
        n-Nashville, TN (WWCR) 36N12'30"-86W53'38"
        nm-NMG New Orleans, LA 29N53'04"-89W56'43"
        no-New Orleans, LA (WRNO) 29N50'10"-90W06'57"
        np-Newport, NC (WTJC) 34N46'41"-76W52'37"
        o-Okeechobee, FL (WYFR) 27N27'30"-80W56'00"
        of-Offutt AFB, NE 41N06'49"-95W55'42"
        q-Monticello, ME (WBCQ) 46N20'30"-67W49'40"
        pg-Punta Gorda, FL (KPK) 26N53'39"-82W03'35"
        pr-Point Reyes, CA 37N55'32"-122W43'52"
        rh-Riverhead, Long Island, NY  40N53-72W38
        rl-Red Lion (York), PA (WINB) 39N54'22"-76W34'56"
        rs-Los Angeles / Rancho Simi, CA (KVOH) 34N15'23"-118W38'29"
        sc-KEBR Sacramento, CA 38N27'46"-121W07'49"
        se-Seattle, WA 48N07'32"-122W15'02"
        sf-San Francisco Radio; one of: dx; HWA-m; ALS-ba; THA-hy
        v-Vado, NM (KJES) 32N08'02"-106W35'24"
        vs-Vashon Island, WA 47N22'15"-122W29'16"
        wa-Washington, DC 38N55-77W03
        wc-"West Coast" Beale AFB Marysville, CA 39N08-121W26
        wg-WGM Fort Lauderdale, FL 26N34-80W05
        wx-WHX Hillsboro, WV 38N16'07"-80W16'07"
        xx-Unknown site
   UZB: Tashkent 41N13-69E09 except:
        a-Tashkent Airport 41N16-69E17
        nu-Nukus, Karakalpakstan 42N29-59E37
        s-Samarkand 39N42-66E59
        ta-Tashkent I/II 41N19-69E15
   VEN: t-El Tigre 08N53-64W16
        y-YVTO Caracas 10N30'13"-66W55'44"
   VTN: b-Buon Me Thuot, Daclac 12N38-108E01
        bt-Ben Thuy 18N39-105E42
        cm-Ca Mau 09N10-105E09
        ct-Can Tho 10N02-105E47
        db-Dien Bien 21N22-103E00
        dn-Da Nang 16N03-105E15
        hc-Ho Chi Minh City 10N45-106E40
        hg-Hon Gai (Ha Long) 20N58-107E03
        hp-Hai Phong 20N52-106E40
        hu-Hue 16N28-107E35
        kg-Kien Giang 09N49-105E07
        L-Son La 21N20-103E55
        m-Hanoi-Metri 21N00-105E47
        mc-Mong Cai 21N32-107E58
        nt-Nha Trang 12N15-109E12
        pr-Phan Rang 11N34-108E59
        pt-Phan Tiet 10N59-108E16
        py-Phu Yen 13N07-109E17
        qp-Qui Phong 13N45-109E13
        s-Hanoi-Sontay 21N12-105E22
        t-Thoi Long / Thoi Hung 10N07-105E34
        vt-Vung Tau 10N21-107E05
        x-Xuan Mai 20N43-105E33
        xx-Secret service site
   VUT: Empten Lagoon 17S45-168E22
   XUU: Unknown site
   YEM: a-Al Hiswah/Aden 12N50-45E02
        s-Sanaa 15N22-44E11
   ZMB: L-Lusaka 15S30-28E15
        m-Makeni Ranch 15S32-28E00
   ZWE: Gweru/Guinea Fowl 19S26-29E51

US Air Force Messages: Sites are ALS-e, ASC, AZR-lj, BIO, G-ct, GUM-an, HWA-hi, I-si, J-yo, PTR-s, USA-a, USA-of, USA-wc
