import re
from pathlib import Path

mapping_text = '''0
Hex: #000000
Color Names:
:black , :black_000
Color Group: gray_and_black
1
Hex: #800000
Color Name:
:maroon
Color Group: red
2
Hex: #008000
Color Name:
:office_green
Color Group: green
3
Hex: #808000
Color Name:
:yellow_003
Color Group: green
4
Hex: #000080
Color Name:
:blue_004
Color Group: blue
5
Hex: #800080
Color Name:
:patriarch
Color Group: purple_violet_and_magenta
6
Hex: #008080
Color Name:
:cyan_006
Color Group: cyan
7
Hex: #c0c0c0
Color Name:
:argent
Color Group: gray_and_black
8
Hex: #808080
Color Names:
:gray , :light_black
Color Group: gray_and_black
9
Hex: #ff0000
Color Names:
:light_red , :red
Color Group: red
10
Hex: #00ff00
Color Names:
:electric_green , :green , :light_green_010
Color Group: green
11
Hex: #ffff00
Color Names:
:light_yellow_011 , :yellow
Color Group: yellow
12
Hex: #0000ff
Color Names:
:blue , :light_blue_012
Color Group: blue
13
Hex: #ff00ff
Color Names:
:fuchsia , :light_magenta_013 , :magenta
Color Group: pink
14
Hex: #00ffff
Color Names:
:aqua , :cyan , :light_cyan_014
Color Group: cyan
15
Hex: #ffffff
Color Names:
:light_white , :white
Color Group: white
16
Hex: #000000
Color Names:
:black , :black_000
Color Group: gray_and_black
17
Hex: #00005f
Color Names:
:fuzzy_wuzzy , :stratos , :very_dark_blue
Color Group: blue
18
Hex: #000087
Color Names:
:dark_blue , :navy_blue
Color Group: blue
19
Hex: #0000af
Color Names:
:carnation_pink , :duke_blue
Color Group: blue
20
Hex: #0000d7
Color Name:
:medium_blue
Color Group: blue
21
Hex: #0000ff
Color Names:
:blue , :light_blue_012
Color Group: blue
22
Hex: #005f00
Color Names:
:camarone , :very_dark_lime_green
Color Group: green
23
Hex: #005f5f
Color Names:
:bangladesh_green , :blue_stone , :dark_slate_gray , :very_dark_cyan
Color Group: green
24
Hex: #005f87
Color Names:
:orient , :sea_blue
Color Group: blue
25
Hex: #005faf
Color Names:
:endeavour , :medium_persian_blue
Color Group: blue
26
Hex: #005fd7
Color Names:
:science_blue , :true_blue
Color Group: blue
27
Hex: #005fff
Color Names:
:blue_ribbon , :brandeis_blue
Color Group: blue
28
Hex: #008700
Color Name:
:ao
Color Group: green
29
Hex: #00875f
Color Names:
:deep_sea , :spanish_viridian
Color Group: green
30
Hex: #008787
Color Name:
:teal
Color Group: cyan
31
Hex: #0087af
Color Name:
:deep_cerulean
Color Group: blue
32
Hex: #0087d7
Color Names:
:blue_cola , :lochmara , :strong_blue
Color Group: blue
33
Hex: #0087ff
Color Names:
:azure , :azure_radiance , :pure_blue
Color Group: blue
34
Hex: #00af00
Color Names:
:dark_lime_green , :islamic_green , :japanese_laurel
Color Group: green
35
Hex: #00af5f
Color Names:
:go_green , :jade
Color Group: green
36
Hex: #00af87
Color Names:
:dark_cyan , :persian_green
Color Group: green
37
Hex: #00afaf
Color Names:
:bondi_blue , :tiffany_blue
Color Group: cyan
38
Hex: #00afd7
Color Name:
:cerulean
Color Group: blue
39
Hex: #00afff
Color Names:
:blue_bolt , :deep_sky_blue
Color Group: blue
40
Hex: #00d700
Color Name:
:strong_lime_green
Color Group: green
41
Hex: #00d75f
Color Name:
:malachite
Color Group: green
42
Hex: #00d787
Color Name:
:caribbean_green_042
Color Group: green
43
Hex: #00d7af
Color Names:
:caribbean_green , :strong_cyan
Color Group: green
44
Hex: #00d7d7
Color Names:
:dark_turquoise , :robins_egg_blue
Color Group: cyan
45
Hex: #00d7ff
Color Name:
:vivid_sky_blue
Color Group: blue
46
Hex: #00ff00
Color Names:
:electric_green , :green , :light_green_010
Color Group: green
47
Hex: #00ff5f
Color Name:
:spring_green_047
Color Group: green
48
Hex: #00ff87
Color Name:
:guppie_green
Color Group: green
49
Hex: #00ffaf
Color Names:
:medium_spring_green , :spring_green
Color Group: green
50
Hex: #00ffd7
Color Names:
:bright_turquoise , :pure_cyan , :sea_green
Color Group: green
51
Hex: #00ffff
Color Names:
:aqua , :cyan , :light_cyan_014
Color Group: cyan
52
Hex: #5f0000
Color Names:
:blood_red , :rosewood , :very_dark_red
Color Group: red
53
Hex: #5f005f
Color Names:
:imperial_purple , :pompadour , :very_dark_magenta
Color Group: purple_violet_and_magenta
54
Hex: #5f0087
Color Names:
:metallic_violet , :pigment_indigo
Color Group: purple_violet_and_magenta
55
Hex: #5f00af
Color Names:
:chinese_purple , :dark_violet
Color Group: purple_violet_and_magenta
56
Hex: #5f00d7
Color Name:
:electric_violet_056
Color Group: purple_violet_and_magenta
57
Hex: #5f00ff
Color Name:
:electric_indigo
Color Group: purple_violet_and_magenta
58
Hex: #5f5f00
Color Names:
:bronze_yellow , :verdun_green , :very_dark_yellow_olive_tone
Color Group: green
59
Hex: #5f5f5f
Color Name:
:scorpion
Color Group: gray_and_black
60
Hex: #5f5f87
Color Names:
:comet , :mostly_desaturated_dark_blue , :ucla_blue
Color Group: blue
61
Hex: #5f5faf
Color Names:
:dark_moderate_blue , :liberty , :scampi
Color Group: blue
62
Hex: #5f5fd7
Color Names:
:indigo , :slate_blue
Color Group: blue
63
Hex: #5f5fff
Color Name:
:cornflower_blue
Color Group: blue
64
Hex: #5f8700
Color Name:
:avocado
Color Group: green
65
Hex: #5f875f
Color Names:
:glade_green , :mostly_desaturated_dark_lime_green , :russian_green
Color Group: green
66
Hex: #5f8787
Color Names:
:juniper , :mostly_desaturated_dark_cyan , :steel_teal
Color Group: cyan
67
Hex: #5f87af
Color Names:
:hippie_blue , :rackley , :steel_blue
Color Group: blue
68
Hex: #5f87d7
Color Names:
:havelock_blue , :moderate_blue , :united_nations_blue
Color Group: blue
69
Hex: #5f87ff
Color Names:
:blueberry , :light_blue
Color Group: blue
70
Hex: #5faf00
Color Names:
:dark_green , :kelly_green
Color Group: green
71
Hex: #5faf5f
Color Names:
:dark_moderate_lime_green , :fern , :forest_green
Color Group: green
72
Hex: #5faf87
Color Names:
:polished_pine , :silver_tree
Color Group: green
73
Hex: #5fafaf
Color Names:
:crystal_blue , :dark_moderate_cyan , :tradewind
Color Group: blue
74
Hex: #5fafd7
Color Names:
:aqua_pearl , :carolina_blue , :shakespeare
Color Group: blue
75
Hex: #5fafff
Color Name:
:blue_jeans
Color Group: blue
76
Hex: #5fd700
Color Names:
:alien_armpit , :harlequin_green , :strong_green
Color Group: green
77
Hex: #5fd75f
Color Name:
:moderate_lime_green
Color Group: green
78
Hex: #5fd787
Color Name:
:caribbean_green_pearl
Color Group: green
79
Hex: #5fd7af
Color Names:
:downy , :eucalyptus
Color Group: green
80
Hex: #5fd7d7
Color Names:
:medium_turquoise , :moderate_cyan , :viking
Color Group: cyan
81
Hex: #5fd7ff
Color Name:
:maya_blue
Color Group: blue
82
Hex: #5fff00
Color Name:
:bright_green
Color Group: green
83
Hex: #5fff5f
Color Name:
:light_lime_green
Color Group: green
84
Hex: #5fff87
Color Name:
:very_light_malachite_green
Color Group: green
85
Hex: #5fffaf
Color Name:
:medium_aquamarine
Color Group: green
86
Hex: #5fffd7
Color Name:
:aquamarine_086
Color Group: cyan
87
Hex: #5fffff
Color Name:
:aquamarine_087
Color Group: cyan
88
Hex: #870000
Color Name:
:deep_red
Color Group: red
89
Hex: #87005f
Color Name:
:french_plum
Color Group: purple_violet_and_magenta
90
Hex: #870087
Color Names:
:fresh_eggplant , :mardi_gras
Color Group: purple_violet_and_magenta
91
Hex: #8700af
Color Names:
:purple , :violet
Color Group: purple_violet_and_magenta
92
Hex: #8700d7
Color Names:
:french_violet , :strong_violet
Color Group: purple_violet_and_magenta
93
Hex: #8700ff
Color Names:
:electric_violet , :pure_violet
Color Group: purple_violet_and_magenta
94
Hex: #875f00
Color Names:
:brown , :gamboge_orange
Color Group: brown
95
Hex: #875f5f
Color Names:
:copper_rose , :deep_taupe , :mostly_desaturated_dark_red
Color Group: brown
96
Hex: #875f87
Color Names:
:chinese_violet , :mostly_desaturated_dark_magenta , :strikemaster
Color Group: purple_violet_and_magenta
97
Hex: #875faf
Color Names:
:dark_moderate_violet , :deluge , :royal_purple
Color Group: purple_violet_and_magenta
98
Hex: #875fd7
Color Names:
:medium_purple , :moderate_violet
Color Group: purple_violet_and_magenta
99
Hex: #875fff
Color Name:
:blueberry_099
Color Group: purple_violet_and_magenta
100
Hex: #878700
Color Names:
:dark_yellow_olive_tone , :olive
Color Group: green
101
Hex: #87875f
Color Names:
:clay_creek , :mostly_desaturated_dark_yellow , :shadow
Color Group: green
102
Hex: #878787
Color Name:
:taupe_gray
Color Group: gray_and_black
103
Hex: #8787af
Color Names:
:cool_grey , :dark_grayish_blue , :shadow_blue
Color Group: blue
104
Hex: #8787d7
Color Names:
:chetwode_blue , :ube
Color Group: blue
105
Hex: #8787ff
Color Name:
:violets_are_blue
Color Group: blue
106
Hex: #87af00
Color Names:
:apple_green , :limeade
Color Group: green
107
Hex: #87af5f
Color Names:
:asparagus , :chelsea_cucumber , :dark_moderate_green
Color Group: green
108
Hex: #87af87
Color Names:
:bay_leaf , :dark_grayish_lime_green , :dark_sea_green
Color Group: green
109
Hex: #87afaf
Color Names:
:dark_grayish_cyan , :gulf_stream , :pewter_blue
Color Group: cyan
110
Hex: #87afd7
Color Names:
:light_cobalt_blue , :polo_blue , :slightly_desaturated_blue
Color Group: blue
111
Hex: #87afff
Color Names:
:french_sky_blue , :malibu
Color Group: blue
112
Hex: #87d700
Color Name:
:pistachio
Color Group: green
113
Hex: #87d75f
Color Name:
:mantis
Color Group: green
114
Hex: #87d787
Color Names:
:pastel_green , :slightly_desaturated_lime_green
Color Group: green
115
Hex: #87d7af
Color Names:
:pearl_aqua , :vista_blue
Color Group: cyan
116
Hex: #87d7d7
Color Names:
:bermuda , :middle_blue_green , :slightly_desaturated_cyan
Color Group: cyan
117
Hex: #87d7ff
Color Names:
:pale_cyan , :very_light_blue
Color Group: cyan
118
Hex: #87ff00
Color Names:
:chartreuse , :pure_green
Color Group: green
119
Hex: #87ff5f
Color Names:
:light_green , :screamin_green
Color Group: green
120
Hex: #87ff87
Color Name:
:very_light_lime_green
Color Group: green
121
Hex: #87ffaf
Color Name:
:mint_green_121
Color Group: green
122
Hex: #87ffd7
Color Names:
:aquamarine , :lime_green
Color Group: cyan
123
Hex: #87ffff
Color Names:
:anakiwa , :electric_blue , :very_light_cyan
Color Group: cyan
124
Hex: #af0000
Color Names:
:bright_red , :dark_candy_apple_red , :dark_red
Color Group: red
125
Hex: #af005f
Color Names:
:dark_pink , :jazzberry_jam
Color Group: red
126
Hex: #af0087
Color Names:
:dark_magenta , :flirt
Color Group: purple_violet_and_magenta
127
Hex: #af00af
Color Name:
:heliotrope_magenta
Color Group: purple_violet_and_magenta
128
Hex: #af00d7
Color Name:
:vivid_mulberry
Color Group: purple_violet_and_magenta
129
Hex: #af00ff
Color Name:
:electric_purple
Color Group: purple_violet_and_magenta
130
Hex: #af5f00
Color Names:
:dark_orange_brown_tone , :rose_of_sharon , :windsor_tan
Color Group: brown
131
Hex: #af5f5f
Color Names:
:dark_moderate_red , :electric_brown , :matrix
Color Group: brown
132
Hex: #af5f87
Color Names:
:dark_moderate_pink , :tapestry , :turkish_rose
Color Group: red
133
Hex: #af5faf
Color Names:
:dark_moderate_magenta , :pearly_purple
Color Group: purple_violet_and_magenta
134
Hex: #af5fd7
Color Name:
:rich_lilac
Color Group: purple_violet_and_magenta
135
Hex: #af5fff
Color Names:
:lavender_indigo , :light_violet
Color Group: purple_violet_and_magenta
136
Hex: #af8700
Color Names:
:dark_goldenrod , :pirate_gold
Color Group: brown
137
Hex: #af875f
Color Names:
:bronze , :dark_moderate_orange , :muesli
Color Group: brown
138
Hex: #af8787
Color Names:
:dark_grayish_red , :english_lavender , :pharlap
Color Group: brown
139
Hex: #af87af
Color Names:
:bouquet , :dark_grayish_magenta , :opera_mauve
Color Group: purple_violet_and_magenta
140
Hex: #af87d7
Color Names:
:lavender , :slightly_desaturated_violet
Color Group: purple_violet_and_magenta
141
Hex: #af87ff
Color Name:
:bright_lavender
Color Group: purple_violet_and_magenta
142
Hex: #afaf00
Color Names:
:buddha_gold , :light_gold
Color Group: brown
143
Hex: #afaf5f
Color Names:
:dark_moderate_yellow , :olive_green
Color Group: green
144
Hex: #afaf87
Color Names:
:dark_grayish_yellow , :hillary , :sage
Color Group: brown
145
Hex: #afafaf
Color Name:
:silver_foil
Color Group: gray_and_black
146
Hex: #afafd7
Color Names:
:grayish_blue , :wild_blue_yonder , :wistful
Color Group: blue
147
Hex: #afafff
Color Names:
:maximum_blue_purple , :melrose
Color Group: blue
148
Hex: #afd700
Color Names:
:rio_grande , :sheen_green , :vivid_lime_green
Color Group: green
149
Hex: #afd75f
Color Names:
:conifer , :june_bud , :moderate_green
Color Group: green
150
Hex: #afd787
Color Names:
:feijoa , :slightly_desaturated_green , :yellow_green
Color Group: green
151
Hex: #afd7af
Color Names:
:grayish_lime_green , :light_moss_green , :pixie_green
Color Group: green
152
Hex: #afd7d7
Color Names:
:crystal , :grayish_cyan , :jungle_mist
Color Group: cyan
153
Hex: #afd7ff
Color Names:
:fresh_air , :pale_blue
Color Group: blue
154
Hex: #afff00
Color Names:
:lime , :spring_bud
Color Group: green
155
Hex: #afff5f
Color Names:
:green_yellow , :inchworm
Color Group: green
156
Hex: #afff87
Color Names:
:mint_green , :very_light_green
Color Group: green
157
Hex: #afffaf
Color Names:
:menthol , :pale_lime_green
Color Group: green
158
Hex: #afffd7
Color Names:
:aero_blue , :magic_mint
Color Group: cyan
159
Hex: #afffff
Color Names:
:celeste , :french_pass
Color Group: cyan
160
Hex: #d70000
Color Names:
:guardsman_red , :rosso_corsa , :strong_red
Color Group: red
161
Hex: #d7005f
Color Names:
:razzmatazz , :royal_red
Color Group: red
162
Hex: #d70087
Color Names:
:mexican_pink , :strong_pink
Color Group: pink
163
Hex: #d700af
Color Name:
:hollywood_cerise_163
Color Group: pink
164
Hex: #d700d7
Color Names:
:deep_magenta , :strong_magenta
Color Group: purple_violet_and_magenta
165
Hex: #d700ff
Color Name:
:phlox
Color Group: purple_violet_and_magenta
166
Hex: #d75f00
Color Names:
:strong_orange , :tenn
Color Group: orange
167
Hex: #d75f5f
Color Names:
:indian_red , :moderate_red , :roman
Color Group: red
168
Hex: #d75f87
Color Names:
:blush , :cranberry , :mystic_pearl
Color Group: red
169
Hex: #d75faf
Color Names:
:hopbush , :moderate_pink , :super_pink
Color Group: pink
170
Hex: #d75fd7
Color Names:
:moderate_magenta , :orchid
Color Group: purple_violet_and_magenta
171
Hex: #d75fff
Color Names:
:heliotrope , :light_magenta
Color Group: pink
172
Hex: #d78700
Color Names:
:chocolate , :harvest_gold , :mango_tango
Color Group: brown
173
Hex: #d7875f
Color Names:
:copperfield , :raw_sienna
Color Group: brown
174
Hex: #d78787
Color Names:
:my_pink , :new_york_pink , :slightly_desaturated_red
Color Group: pink
175
Hex: #d787af
Color Names:
:can_can , :middle_purple , :slightly_desaturated_pink
Color Group: pink
176
Hex: #d787d7
Color Names:
:deep_mauve , :light_orchid , :slightly_desaturated_magenta
Color Group: pink
177
Hex: #d787ff
Color Names:
:bright_lilac , :very_light_violet
Color Group: pink
178
Hex: #d7af00
Color Names:
:goldenrod , :mustard_yellow
Color Group: brown
179
Hex: #d7af5f
Color Names:
:earth_yellow , :moderate_orange
Color Group: brown
180
Hex: #d7af87
Color Names:
:slightly_desaturated_orange , :tan
Color Group: brown
181
Hex: #d7afaf
Color Names:
:clam_shell , :grayish_red , :pale_chestnut
Color Group: brown
182
Hex: #d7afd7
Color Names:
:grayish_magenta , :pink_lavender , :thistle
Color Group: pink
183
Hex: #d7afff
Color Names:
:mauve , :pale_violet
Color Group: purple_violet_and_magenta
184
Hex: #d7d700
Color Names:
:citrine , :corn , :strong_yellow
Color Group: yellow
185
Hex: #d7d75f
Color Names:
:chinese_green , :moderate_yellow , :tacha
Color Group: yellow
186
Hex: #d7d787
Color Names:
:deco , :medium_spring_bud , :slightly_desaturated_yellow
Color Group: yellow
187
Hex: #d7d7af
Color Names:
:grayish_yellow , :green_mist , :pastel_gray
Color Group: green
188
Hex: #d7d7d7
Color Name:
:light_silver
Color Group: gray_and_black
189
Hex: #d7d7ff
Color Names:
:fog , :pale_lavender , :very_pale_blue
Color Group: blue
190
Hex: #d7ff00
Color Names:
:chartreuse_yellow , :pure_yellow
Color Group: yellow
191
Hex: #d7ff5f
Color Names:
:canary , :maximum_green_yellow
Color Group: yellow
192
Hex: #d7ff87
Color Names:
:honeysuckle , :mindaro
Color Group: yellow
193
Hex: #d7ffaf
Color Names:
:pale_green , :reef , :tea_green
Color Group: green
194
Hex: #d7ffd7
Color Names:
:beige , :nyanza , :snowy_mint , :very_pale_lime_green
Color Group: green
195
Hex: #d7ffff
Color Names:
:light_cyan , :oyster_bay , :very_pale_cyan
Color Group: green
196
Hex: #ff0000
Color Names:
:light_red , :red
Color Group: red
197
Hex: #ff005f
Color Names:
:vivid_raspberry , :winter_sky
Color Group: red
198
Hex: #ff0087
Color Names:
:bright_pink , :rose
Color Group: pink
199
Hex: #ff00af
Color Names:
:fashion_fuchsia , :hollywood_cerise , :pure_pink
Color Group: pink
200
Hex: #ff00d7
Color Name:
:pure_magenta
Color Group: pink
201
Hex: #ff00ff
Color Names:
:fuchsia , :light_magenta_013 , :magenta
Color Group: pink
202
Hex: #ff5f00
Color Names:
:blaze_orange , :orange_red , :vivid_orange
Color Group: orange
203
Hex: #ff5f5f
Color Names:
:bittersweet , :pastel_red
Color Group: red
204
Hex: #ff5f87
Color Names:
:strawberry , :wild_watermelon
Color Group: red
205
Hex: #ff5faf
Color Names:
:hot_pink , :light_pink
Color Group: pink
206
Hex: #ff5fd7
Color Names:
:light_deep_pink , :purple_pizzazz
Color Group: pink
207
Hex: #ff5fff
Color Names:
:pink_flamingo , :shocking_pink
Color Group: pink
208
Hex: #ff8700
Color Names:
:american_orange , :dark_orange , :flush_orange
Color Group: orange
209
Hex: #ff875f
Color Names:
:coral , :salmon
Color Group: orange
210
Hex: #ff8787
Color Names:
:tulip , :very_light_red , :vivid_tangerine
Color Group: red
211
Hex: #ff87af
Color Names:
:pink_salmon , :tickle_me_pink
Color Group: pink
212
Hex: #ff87d7
Color Names:
:lavender_rose , :princess_perfume , :very_light_pink
Color Group: pink
213
Hex: #ff87ff
Color Names:
:blush_pink , :fuchsia_pink , :very_light_magenta
Color Group: pink
214
Hex: #ffaf00
Color Names:
:chinese_yellow , :orange , :pure_orange , :yellow_sea
Color Group: orange
215
Hex: #ffaf5f
Color Names:
:light_orange , :rajah , :texas_rose
Color Group: orange
216
Hex: #ffaf87
Color Names:
:hit_pink , :macaroni_and_cheese , :very_light_orange
Color Group: orange
217
Hex: #ffafaf
Color Names:
:melon , :pale_red_pink_tone , :sundown
Color Group: orange
218
Hex: #ffafd7
Color Names:
:cotton_candy , :lavender_pink , :pale_pink
Color Group: pink
219
Hex: #ffafff
Color Names:
:pale_magenta , :rich_brilliant_lavender , :shampoo
Color Group: pink
220
Hex: #ffd700
Color Name:
:gold
Color Group: yellow
221
Hex: #ffd75f
Color Names:
:dandelion , :naples_yellow
Color Group: yellow
222
Hex: #ffd787
Color Names:
:grandis , :jasmine , :khaki
Color Group: brown
223
Hex: #ffd7af
Color Names:
:caramel , :moccasin , :pale_orange
Color Group: brown
224
Hex: #ffd7d7
Color Names:
:cosmos , :misty_rose , :very_pale_red_pink_tone
Color Group: pink
225
Hex: #ffd7ff
Color Names:
:bubble_gum , :pink_lace , :very_pale_magenta
Color Group: pink
226
Hex: #ffff00
Color Names:
:light_yellow_011 , :yellow
Color Group: yellow
227
Hex: #ffff5f
Color Names:
:laser_lemon , :light_yellow
Color Group: yellow
228
Hex: #ffff87
Color Names:
:dolly , :pastel_yellow , :very_light_yellow
Color Group: yellow
229
Hex: #ffffaf
Color Names:
:calamansi , :pale_yellow , :portafino
Color Group: yellow
230
Hex: #ffffd7
Color Names:
:cream , :cumulus , :very_pale_yellow
Color Group: white
231
Hex: #ffffff
Color Names:
:light_white , :white
Color Group: white
232
Hex: #080808
Color Name:
:vampire_black
Color Group: gray_and_black
233
Hex: #121212
Color Names:
:chinese_black , :cod_gray
Color Group: gray_and_black
234
Hex: #1c1c1c
Color Name:
:eerie_black
Color Group: gray_and_black
235
Hex: #262626
Color Name:
:raisin_black
Color Group: gray_and_black
236
Hex: #303030
Color Name:
:dark_charcoal
Color Group: gray_and_black
237
Hex: #3a3a3a
Color Names:
:black_olive , :mine_shaft
Color Group: gray_and_black
238
Hex: #444444
Color Name:
:outer_space
Color Group: gray_and_black
239
Hex: #4e4e4e
Color Names:
:dark_liver , :tundora
Color Group: gray_and_black
240
Hex: #585858
Color Name:
:davys_grey
Color Group: gray_and_black
241
Hex: #626262
Color Names:
:granite_gray , :very_dark_gray
Color Group: gray_and_black
242
Hex: #6c6c6c
Color Names:
:dim_gray , :dove_gray
Color Group: gray_and_black
243
Hex: #767676
Color Names:
:boulder , :sonic_silver
Color Group: gray_and_black
244
Hex: #808080
Color Names:
:gray , :light_black
Color Group: gray_and_black
245
Hex: #8a8a8a
Color Name:
:philippine_gray
Color Group: gray_and_black
246
Hex: #949494
Color Name:
:dusty_gray
Color Group: gray_and_black
247
Hex: #9e9e9e
Color Name:
:spanish_gray
Color Group: gray_and_black
248
Hex: #a8a8a8
Color Names:
:dark_gray , :quick_silver
Color Group: gray_and_black
249
Hex: #b2b2b2
Color Names:
:philippine_silver , :silver_chalice
Color Group: gray_and_black
250
Hex: #bcbcbc
Color Name:
:silver
Color Group: gray_and_black
251
Hex: #c6c6c6
Color Name:
:silver_sand
Color Group: gray_and_black
252
Hex: #d0d0d0
Color Names:
:american_silver , :light_gray
Color Group: gray_and_black
253
Hex: #dadada
Color Names:
:alto , :gainsboro
Color Group: gray_and_black
254
Hex: #e4e4e4
Color Names:
:mercury , :platinum
Color Group: gray_and_black
255
Hex: #eeeeee
Color Names:
:bright_gray , :gallery , :very_light_gray
Color Group: gray_and_black'''

chosen_names = {
    17: 'VERY_DARK_BLUE',
}

line_pattern = re.compile(r'^(\d+)\nHex: #(\w{6})\nColor Names?:\n(.+?)\nColor Group: (\w+)$', re.MULTILINE | re.DOTALL)
entries = []
for match in line_pattern.finditer(mapping_text):
    idx = int(match.group(1))
    hex_code = '#' + match.group(2).lower()
    names_line = match.group(3).strip()
    group = match.group(4).strip().upper()
    candidates = [name.strip().lstrip(':') for name in names_line.split(',') if name.strip()]
    candidates = [name.upper() for name in candidates]
    if idx in chosen_names:
        name = chosen_names[idx]
    elif len(candidates) == 1:
        name = candidates[0]
    else:
        name = hex_code.upper()
    entries.append((idx, name, group, hex_code))

entries.sort(key=lambda x: x[0])

output_lines = ['static const ColorPaletteEntry g_color_palette_entries[256] = {']
for idx, name, group, hex_code in entries:
    output_lines.append(f'    {{ {idx}, "{name}", COLOR_PALETTE_GROUP_{group}, "{hex_code}" }},')
output_lines.append('};')

source_text = Path('src/color_palette.c').read_text(encoding='utf-8')
new_text, count = re.subn(r'static const ColorPaletteEntry g_color_palette_entries\[256\] = \{.*?\};', '\n'.join(output_lines), source_text, flags=re.DOTALL)
if count != 1:
    raise RuntimeError('Failed to replace palette block')
Path('src/color_palette.c').write_text(new_text, encoding='utf-8')
print('Rebuilt palette block with current choices and preserved ambiguous entries as hex names.')
