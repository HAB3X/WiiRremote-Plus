/*
   talk_lines.inc - the pieces the dealers build their lines from.
   LN(who, what for, mood from, mood to, needs, text)
   mood: -2 furious (you're beating them) ... 0 neutral ... 2 gloating (they're beating you)
   [a|b|c] = pick one, [a|] = maybe, {A} = maybe your name, {N} = your name, other {..} = details.
   Included in talk.inc. WiiRremote Plus by Habex. GPL-3.0.
*/
#define MO W_MOTH
#define LY W_LYNX
#define CR W_CROW
#define SQ W_SQUID
#define VX W_VIXEN
#define VP W_VIPER
#define AN W_ANY
static const Line LINES[] = {
/* ================================================================= */
/* PHANTOM MOTH (blackjack, the floor)                               */
/* ================================================================= */
LN(MO, I_HELLO, -2, 2, F_FIRST, "[Ah.|Mm.|So.] [A new mind at my table.|Someone new. How... refreshing.|I don't know you yet{A}. That will change.] [Sit.|Sit down.|Please. Sit.]"),
LK(MO, I_HELLO, -2, 2, F_FIRST, GK_BJ, "[Welcome|Come in|Sit], [stranger|traveller]. [I deal blackjack.|Twenty one is my game.] [Among other things.|And I read people. Mostly the second one.|The cards and I have an understanding.]"),
LN(MO, I_HELLO, -2, 2, F_BACK, "[Back again{A}.|You came back{A}.|There you are{A}.] [I knew you would.|I felt you coming up the stairs.|I was just thinking about you. I'm always just thinking about you.]"),
LN(MO, I_HELLO, -2, 2, F_BACK, "[Ah.|Mm.] [{N}. My table missed you.|The chair remembered your shape{A}.] [Sit.|Shall we?]"),
LN(MO, I_HELLO, -2, 2, F_BACK | F_RELHI, "[You.|Oh. You.] [The one who keeps taking my chips.|I've been practising since last time{A}.|Sit down. I've been waiting to get even.]"),
LN(MO, I_HELLO, -2, 2, F_BACK | F_RELLO, "[My favourite donor.|Welcome back{A}.] [Your chips missed me.|Come to give me more?|The seat's still warm from last time.]"),
LN(MO, I_HELLO, -2, 2, F_RETURN, "[Couldn't stay away?|Back so soon{A}?|Ah. You again. Tonight, I mean.] [Sit.|The cards kept your seat warm.|I didn't shuffle. I knew you'd be back.]"),
LN(MO, I_HELLO, -2, 2, F_GUP, "[I heard you took {gnet} from {other}.|{other} won't stop complaining about you.|Word travels{A}. {other}, {gnet} chips.] [Let's see you try that on me.|That won't happen here.|I'm not {other}.]"),
LN(MO, I_HELLO, -2, 2, F_GDOWN, "[{other} tells me you left {gnet} chips at the {ogame}.|I hear {other} had a good night. Thanks to you.] [Come to win it back?|Don't worry. I'll be gentle. I won't be gentle.]"),

LN(MO, I_BJSIT, -2, 2, 0, "[Blackjack.|My table.|Twenty one.] [My cards. Pick a mode.|You know I read cards. And people.|Or ruin. Choose.|Sit where you like. I'll find you.]"),
LN(MO, I_BJSIT, -2, 2, F_RETURN, "[Back to my cards{A}.|More blackjack?|Again?] [Good. Pick your poison.|They missed you. I didn't.]"),
LN(MO, I_WIN, -2, 2, 0, "[Fine.|Hm.|Very well.] [Take it{A}.|It's yours.|Enjoy it while it lasts.]"),
LN(MO, I_WIN, 0, 2, 0, "[Well played{A}.|Nicely done.|A win.] [Don't get used to it.|I let that one go.|Consider it a gift.|Luck. Nothing more.]"),
LN(MO, I_WIN, -2, -1, 0, "[Again?|Of course.|That's...] [How do you keep doing that?|I'm starting to hate that sound.|Stop smiling{A}. I can hear you smiling.]"),
LK(MO, I_WIN, -2, 2, F_BJ, GK_BJ, "[Blackjack.|Twenty one in two cards.|Twenty one, right off the top.] [How lucky for you{A}.|I felt that one coming. I dealt it anyway.|Enjoy it. The next ace is mine.]"),
LK(MO, I_WIN, -2, -1, F_BJ, GK_BJ, "[Another blackjack?|Blackjack. Again.] [Where are you hiding them{A}?|The shoe is conspiring against me.|Who taught you to do that?]"),
LK(MO, I_WIN, -2, 2, F_BJ | F_AGAIN, GK_BJ, "[That's your {ord} blackjack{A}.|Blackjack number {n} {tod}.] [I'm counting.|Someone up there likes you. Not me.|I don't like this pattern.]"),
LK(MO, I_WIN, -2, 2, F_DBUST, GK_BJ, "[I bust.|{dv}. Too many.|Over twenty one. Me. How embarrassing.] [The cards betrayed me.|Don't look at me like that.|That was the shoe's fault. Not mine.]"),
LK(MO, I_WIN, -2, 2, F_DBUST, GK_BJ, "I drew to {dv}. [Greedy of me.|I never learn.|...Say nothing{A}.]"),
LN(MO, I_WIN, -2, 2, F_DOUBLE, "[You doubled and it paid.|Double down, double win{A}.] [Brave.|I felt your heart race when you did that.|Bold. I respect bold. I hate it, but I respect it.]"),
LN(MO, I_WIN, -2, 2, F_SPLIT, "[Splitting paid off.|Two hands, twice the trouble for me.] [Clever{A}.|I didn't see that coming. I see everything coming.]"),
LK(MO, I_WIN, -2, 2, F_CLOSE, GK_BJ, "[{pv} to my {dv}.|By one.|One point{A}.] [That close.|You won by a hair. I'll remember that hair.]"),
LN(MO, I_WIN, -2, 2, F_COMEBACK, "[Finally.|There it is.|A win, at last.] [I was beginning to feel sorry for you{A}.|Enjoy it. You've earned one.|Don't cry. It's only a win.]"),
LN(MO, I_WIN, -1, 2, F_STREAK, "[{pst} in a row.|Another one.|Again.] [This is becoming a habit{A}.|Keep this up and I'll have to start trying.|Hm. Are you counting cards? You're counting cards.]"),
LK(MO, I_WIN, -2, -2, F_STREAK, GK_BJ, "[{pst} in a row?!|Enough!|How?!] [I read your mind{A}! This should not be possible!|These cards are mine!|Stop. Winning. I am warning you.|The shoe is cursed. You cursed it!]"),
LN(MO, I_WIN, -2, 2, F_HUGE, "[That's a big pile you just took{A}.|{net} chips. From me.] [I'll want those back.|Put them somewhere safe. Somewhere I can't see. There's nowhere I can't see.]"),

LK(MO, I_LOSE, -2, 2, 0, GK_BJ, "[The house wins.|Mine.|That one's mine.] [As I foresaw.|I saw it before the shoe did.|Thank you{A}.]"),
LN(MO, I_LOSE, 1, 2, 0, "[Ha.|Mm.|Oh dear.] [Your chips were never really yours{A}.|You never learn, do you?|I can hear your heart sinking.|Another one for me.]"),
LN(MO, I_LOSE, -2, -1, 0, "[Finally.|At last.|There.] [Order is restored.|The cards remember who they belong to.|That's more like it{A}.]"),
LK(MO, I_LOSE, -2, 2, F_BUST, GK_BJ, "[{pv}. Bust.|Over twenty one{A}.|Too many.] [You hit on {pb}. I knew you would.|{card}. It had to be {card}.|Greedy. I like greedy.]"),
LK(MO, I_LOSE, -2, 2, F_BUST | F_AGAIN, GK_BJ, "[That's {n} busts {tod}{A}.|Bust number {n}.|Busted again.] [I'm keeping a list.|Would you try standing? Just once? For me?|The shoe loves you. In its way.]"),
LK(MO, I_LOSE, -2, 2, F_DBJ, GK_BJ, "[Blackjack.|Ace and a face.|Twenty one. Mine.] [Sorry{A}.|I'm not sorry.|Did you feel that one coming? I did.]"),
LN(MO, I_LOSE, -2, 2, F_DOUBLE, "[You doubled into that{A}.|Doubled and lost.|Twice the bet, twice the pain.] [I felt your hope. It was delicious.|Next time ask me first. I'll lie.]"),
LN(MO, I_LOSE, -2, 2, F_SURR, "[Surrender.|You gave up.|Half back.] [Wise{A}. Cowardly, but wise.|Running away already?|I'd have beaten you anyway.]"),
LK(MO, I_LOSE, -2, 2, F_CLOSE, GK_BJ, "[{dv} to your {pv}.|One point.|By one{A}.] [So close. I love close.|A hair. My hair.]"),
LN(MO, I_LOSE, 0, 2, F_DSTREAK, "[{dst} in a row{A}.|That's {dst}.|Another.] [Should I let you win one? No.|Are you even trying?|My deck plays better than you.]"),
LN(MO, I_LOSE, -2, 2, F_HUGE, "[{net} chips.|That was a big one{A}.] [They're in my tray now.|They make such a lovely sound when they're mine.]"),

LN(MO, I_PUSH, -2, 2, 0, "[A push.|Even.|Neither of us.] [How dull.|Neither of us wins. Neither of us leaves.|We'll call it a draw. For now.]"),
LK(MO, I_PUSH, -2, 2, F_TIE, GK_BJ, "[{pv} each.|The same number.] [Great minds{A}.|Stop copying me.|The cards couldn't choose between us. I can.]"),

LN(MO, I_DEALT, -2, 2, F_PAIR, "[A pair of {prank}.|Twins.] [Thinking about splitting{A}?|Split them. Or don't. I already know what you'll do.]"),
LN(MO, I_DEALT, -2, 2, F_TWENTY, "[Twenty.|Two pictures.|A nice twenty{A}.] [Don't get greedy.|Don't you dare hit that.|Pretty. Very pretty.]"),
LN(MO, I_DEALT, -2, 2, F_LOW, "[{pv}{A}.|{pv}. Interesting.] [A good number to double on.|I can feel you thinking about doubling.]"),
LN(MO, I_DEALT, -2, 2, F_UPACE, "[I show an ace.|An ace, face up.] [Nervous{A}?|Don't worry. Worry.|Care to insure your hopes?]"),
LN(MO, I_DEALT, -2, 2, F_UPWEAK, "[I'm showing {aup}.|{aup}. A weak card.] [Don't get excited{A}.|You're hoping I bust. I can hear it.]"),
LN(MO, I_DEALT, -2, 2, F_STIFF, "[{pv} against my {aup}.|Ugly hand{A}.|{pv}. Hm.] [Tough spot.|I love this hand. For me.|What will you do? I know what you'll do.]"),

LN(MO, I_DECIDE, -2, 2, F_WRONG, "[{acting} on {pv}{A}?|You'll {act} on {pv}?|Really? {act}?] [Bold.|The book says otherwise. I burned the book.|Interesting choice. Wrong, but interesting.]"),
LN(MO, I_DECIDE, -2, 2, F_WRONG, "[Hm. I wouldn't.|Are you sure{A}?|...Interesting.] [But I'm not you. Thankfully.|Go on then.]"),
LN(MO, I_DECIDE, -2, 2, F_DOUBLE, "[Doubling{A}.|Double. Brave.|Oh, doubling.] [I felt your pulse jump.|Let's see.|Everything on one card.]"),
LN(MO, I_DECIDE, -2, 2, F_SPLIT, "[Splitting.|Two hands now{A}.|Divide and conquer.] [Twice the chance. Twice the trouble.|Greedy. Good.]"),
LN(MO, I_DECIDE, -2, 2, F_HITHIGH, "[Hitting {pv}?|On {pv}{A}?] [Brave. Or foolish. Same thing at this table.|I admire the confidence.]"),

LN(MO, I_IDLE, -2, 2, 0, "[Take your time.|No rush{A}.] [I have all night. I have every night.|The cards aren't going anywhere. Neither are you.]"),
LN(MO, I_IDLE, -2, 2, 0, "[Nervous?|Hands shaking{A}?] [Your hands say yes.|Relax. It's only chips. For now.]"),
LK(MO, I_IDLE, -2, 2, 0, GK_BJ, "[The shoe is getting lighter.|Half the shoe is gone.] [Can you feel it{A}?|Something good is left in there. For someone.]"),
LN(MO, I_IDLE, -2, 2, 0, "[Place a bet.|Go on{A}. Bet.|Bet something.] [I'm curious what you'll do.|Any bet. I just like watching you decide.]"),
LN(MO, I_IDLE, -2, 2, 0, "[You're thinking about leaving.|Thinking of going{A}?] [Don't.|Stay. One more.|Nobody leaves. Well. Some do. Not happily.]"),
LN(MO, I_IDLE, -2, 2, 0, "[Do you hear that?|Listen.] [No? Good.|That's the chandelier. It moves when I'm pleased.|...Nothing. Never mind.]"),
LN(MO, I_IDLE, -2, -1, 0, "[I'm not upset.|I'm fine{A}.] [I'm perfectly calm.|The shaking is the chandelier. Not me.]"),
LN(MO, I_IDLE, 1, 2, 0, "[I'm enjoying tonight{A}.|What a pleasant evening.] [Aren't you?|Your chips are.]"),
LN(MO, I_IDLE, -2, 2, F_LONG, "[Still there{A}?|Hello?|...] [I can wait. I can wait forever.|Did you fall asleep? I'd know if you had.|I'm going to start reading your mind out of boredom.]"),
LN(MO, I_IDLE, -2, 2, F_BROKE, "[Running low{A}.|Not many chips left.] [I can smell it.|Careful. Or don't. I enjoy both.]"),
LN(MO, I_IDLE, -2, 2, F_RICH, "[So many chips{A}.|That pile of yours.] [It's making me hungry.|Let me help you carry it.]"),
LN(MO, I_IDLE, -2, 2, F_LATE, "[It's late{A}.|Past midnight.] [Shouldn't you be asleep?|Everyone else in your house is asleep. Aren't they?]"),
LK(MO, I_IDLE, -2, 2, F_MORNING, GK_BJ, "[Blackjack in the morning{A}?|The sun's barely up.] [I respect it.|I never sleep. You should.]"),

LN(MO, I_OPEN, -2, 2, 0, "[Hm.|Mm.|Ah.|So.|Well.|...|(tilts his head)]"),
LN(MO, I_OPEN, -2, -2, 0, "[Grr.|(the chandelier shakes)|Enough.]"),
LN(MO, I_OPEN, 1, 2, 0, "[Ha.|Oh.|(a soft laugh)|Heh.]"),
LN(MO, I_TAG, -2, 2, 0, "[Deal.|Go on.|Next.|I'm watching.]"),
LN(MO, I_TAG, 1, 2, 0, "[Isn't this fun?|Again?|More?]"),
LN(MO, I_TAG, -2, -1, 0, "[...I'm fine.|Deal. Now.|Next hand.]"),
LN(MO, I_AFTER, -2, 2, 0, "[...|Hm.] [Don't look at me like that.|I'm not angry. I'm never angry.|That was a joke{A}. You may laugh.|Where was I.]"),
LN(MO, I_RESUME, -2, 2, 0, "[Anyway.|Where was I.|As I was saying...|Now.]"),
LN(MO, I_MEM, -2, 2, M_PEAK, "[You were up {mem} earlier{A}.|An hour ago you had {mem}.|Remember when you had {mem}?] [Good times.|Where did it all go?|I remember. I remember everything.]"),
LN(MO, I_MEM, -2, 2, M_HANDS, "[That's {hands} hands{A}.|{hands} hands at my table.] [Who's counting? Me.|You're a regular now.|I know how you play now. Every habit.]"),
LN(MO, I_MEM, -2, 2, M_BEST, "[Still thinking about that {mem} win{A}?|That {mem} win earlier.] [I am.|It won't happen again.]"),
LK(MO, I_MEM, -2, 2, M_BUSTS, GK_BJ, "[{n} busts {tod}.|You've busted {n} times.] [I've been counting{A}.|Standing is also an option. Just so you know.]"),

/* his slips before the mask comes off */
LN(MO, I_HINT, -2, 2, H_NAME, "[Your turn{A}.|Go on, {N}.] [...What? Did you not tell me your name?|...Hm? Oh. Your name. Lucky guess.]"),
LN(MO, I_HINT, -2, 2, H_SAVES, "[Lot of hours on that Wii of yours.|You've played a lot of games on that Wii.] [...Or so I imagine.|Just a feeling.]"),
LN(MO, I_HINT, -2, 2, H_BACKUP, "[Your SD card feels heavy tonight.|Something heavy on that SD card.] [Never mind. Deal.|Forget I said that.]"),
LN(MO, I_HINT, -2, 2, H_MANY, "[Who else plays on this Wii?|Busy house, isn't it{A}?] [...Forget I asked.|Just a guess.]"),
LN(MO, I_HINT, -2, 2, H_FEW, "[Quiet in your house, isn't it?|Just you on that Wii. Mostly.] [...Just a guess.|Don't ask how I know. I don't know. Probably.]"),
LN(MO, I_HINT, -2, 2, H_ONLINE, "[Is that your Wi-Fi humming?|Your Wii's been out on the internet.] [No? My mistake.|...Or I'm imagining things.]"),
LN(MO, I_HINT, -2, 2, H_FIT, "[You've been exercising.|Stand up straight{A}.] [...Your posture. That's all. Just your posture.|Wii Fit would be proud. What? Nothing.]"),
LN(MO, I_HINT, -2, 2, H_LAST, "[You were playing something before this.|How was {last}?] [I can still smell it on you.|...I mean. Whatever you were playing.]"),
LN(MO, I_HINT, -2, 2, H_EMPTY, "[Strange.|Hm.] [I usually know something about people by now.|You're very quiet. Inside, I mean.|Something about you is... blank.]"),
LN(MO, I_MASK, -2, 2, 0, "[Enough small talk{A}.|Stop.|The mask comes off now.] [Let me show you what I really see.|Look at me. I know you. I know everything about you.|Let's see what your Wii has been telling me.]"),
LN(MO, I_MASK, -2, 2, 0, "[You think this is about cards{A}?|Put your hands where I can see them.] [I am going to read you now.|Hold still. This won't hurt. Much.]"),
LN(MO, I_MASK, -2, 2, H_EMPTY, "[Enough.|Stop playing.] [I'm going to read you now, {N}. Properly.|Let's see what's in there. If anything.]"),
LN(MO, I_READIN, -2, 2, 0, "[And...|Wait. There's more.|Mm. Something else.|Hold still.|Your Wii is talking again.]"),
LN(MO, I_READIN, -2, 2, H_EMPTY, "[I try again...|Let me look deeper...|Harder...]"),
LN(MO, I_CUTIN, -2, 2, 0, "[(from across the floor)|(somewhere behind you)|(a voice from the dark)] [I'm still watching you{A}.|Don't get too comfortable.|Having fun with my staff?|I can hear your chips from here.]"),
LN(MO, I_FLOOR, -2, 2, 0, "[Back on my floor{A}.|There you are.] [How was the {ogame}?|Did {other} treat you well?|Where to next?]"),
LN(MO, I_FLOOR, -2, 2, F_GUP, "[You took {gnet} off {other}.|{other} is sulking. {gnet} chips.] [I'll have a word with {other}.|Don't think I didn't notice{A}.]"),
LN(MO, I_FLOOR, -2, 2, F_GDOWN, "[{other} tells me you left {gnet} behind.|Down {gnet} at the {ogame}?] [My staff do such good work.|Come to my table. I'll make it worse.]"),

/* ================================================================= */
/* TRIGGER LYNX (roulette): a flashy showman croupier                 */
/* ================================================================= */
LK(LY, I_HELLO, -2, 2, F_FIRST, GK_ROUL, "[Howdy.|Well now.|Evenin'{A}.] [Name's Trigger Lynx. I run the wheel.|They call me Trigger Lynx. Fastest spin in the building.] [Pull up a stool.|Thirty-seven pockets. One of 'em's got your name on it.]"),
LK(LY, I_HELLO, -2, 2, F_BACK, GK_WHEEL, "[Well, look who's back.|Back at my wheel{A}?|Howdy again, partner.] [Wheel's been waitin' on you.|Pull up a stool.]"),
LK(LY, I_HELLO, -2, 2, F_BACK | F_RELHI, GK_ROUL, "[Well if it ain't the one who cleaned out my wheel last time.|You again. Lucky number person.] [I've been practisin' my spin{A}.|Let's see if it wasn't a fluke.]"),
LK(LY, I_HELLO, -2, 2, F_BACK | F_RELLO, GK_WHEEL, "[Easy money walks in again.|Howdy, generous.] [Wheel remembers you{A}. Fondly.|Sit a spell.]"),
LK(LY, I_HELLO, -2, 2, F_RETURN, GK_WHEEL, "[Back for another round{A}?|Couldn't stay away from the wheel, huh?] [Wheel's still warm.|Didn't even have time to wipe the table.]"),
LK(LY, I_HELLO, -2, 2, F_GUP, GK_WHEEL, "[Heard you took {gnet} off {other}.|Word travels, partner. {other}, {gnet} chips.] [Don't try that here.|My wheel ain't so generous.]"),
LK(LY, I_HELLO, -2, 2, F_GDOWN, GK_WHEEL, "[{other} says you left {gnet} chips over there.|Heard {other} had a good night on you.] [Wheel's kinder. Sometimes.|Let's see if your luck turned.]"),
LN(LY, I_WIN, -2, 2, 0, "[Hah!|Well I'll be.|Look at that{A}.] [Winner winner.|Pay the lady. Or the fella. Whoever you are.|Don't let it go to your head.]"),
LK(LY, I_WIN, -1, 2, 0, GK_WHEEL, "[There she goes.|Wheel smiled on you.] [Enjoy it{A}.|Good pick.]"),
LK(LY, I_WIN, -2, -1, 0, GK_WHEEL, "[Again?|Hmph.|Tarnation.] [Beginner's luck don't last{A}.|Wheel's got a crush on you.|I'm gonna have to spin harder.]"),
LK(LY, I_WIN, -2, 2, F_SPECIAL, GK_ROUL, "[{num}! Straight up!|Right on {num}.|Single number. {num}.] [Thirty-five to one{A}. Whoo!|I tip my hat. Just this once.|Somebody ring a bell.]"),
LN(LY, I_WIN, -2, 2, F_HUGE, "[That's a fat stack you just pulled{A}.|{net} chips. Somebody call the manager.] [Better guard it.|Don't spend it all at the bar.]"),
LK(LY, I_WIN, -2, -1, F_STREAK, GK_WHEEL, "[Somebody check this wheel!|Nobody's that lucky{A}.|Alright. {pst} in a row. That's enough now.]"),
LN(LY, I_WIN, -2, 2, F_COMEBACK, "[There it is.|Finally hit somethin'.] [Knew you had it in you, kid.|Took you long enough.]"),
LK(LY, I_LOSE, -2, 2, 0, GK_ROUL, "[Ooh.|Not this time{A}.|Nope.] [Wheel had other ideas.|Should've picked another number.|Round and round she goes.]"),
LK(LY, I_LOSE, 1, 2, 0, GK_WHEEL, "[Hah.|Heh.] [Another one for the house{A}.|Keep 'em comin'.|Wheel's hungry {tod}.]"),
LK(LY, I_LOSE, -2, -1, 0, GK_WHEEL, "[About time.|Now we're talkin'.] [Wheel remembered who it works for.|Evens things up a little{A}.]"),
LK(LY, I_LOSE, -2, 2, F_ZERO, GK_ROUL, "[Zero!|Green zero.|The house number.] [Nobody expects the green{A}.|That little green pocket gets everybody.|Ol' zero. My best friend.]"),
LK(LY, I_LOSE, -2, 2, F_CLOSE, GK_ROUL, "[{num}, right next to yours{A}.|One pocket over.|Missed by a hair. {num}.] [That's roulette.|Close only counts in horseshoes.]"),
LK(LY, I_LOSE, -2, 2, F_DSTREAK, GK_ROUL, "[That's {dst} spins in a row{A}.|Wheel's cold on you {tod}.|{dst} straight.] [Maybe try another number.|House always wins in the end.]"),
LK(LY, I_LOSE, -2, 2, F_HUGE, GK_WHEEL, "[Ouch. {net} chips.|That was a big one{A}.] [The wheel was hungry.|Chin up.]"),
LN(LY, I_PUSH, -2, 2, 0, "[Break even.|Came out square{A}.] [Nobody's cryin'.]"),
LN(LY, I_IDLE, -2, 2, 0, "[You hear that click{A}?|Listen to her tick.] [Prettiest sound in the building.]"),
LK(LY, I_IDLE, -2, 2, 0, GK_WHEEL, "[Take your time.|No rush.] [Wheel ain't goin' anywhere.]"),
LK(LY, I_IDLE, -2, 2, 0, GK_WHEEL, "[Watch this.|Hold still.] [(spins the ball on one finger) Still got it.|(rolls a chip across his knuckles) Pretty, ain't it?]"),
LK(LY, I_IDLE, -2, 2, 0, GK_ROUL, "[Red or black{A}?|Pick a number. Any number.] [I got a feelin' about {num}. Don't listen to me.|Trust your gut. Mine's wrong half the time.]"),
LK(LY, I_IDLE, -2, 2, F_LONG, GK_WHEEL, "[You fall asleep on me{A}?|Tumbleweeds rollin' through here.] [Wheel's gettin' cold.]"),
LN(LY, I_IDLE, -2, 2, F_LATE, "[Late night{A}.|Past midnight.] [My kinda hours.|Best crowd's always after midnight.]"),
LN(LY, I_IDLE, -2, 2, F_BROKE, "[Runnin' low{A}.|Few chips left.] [Make 'em count.|Pick careful now.]"),
LK(LY, I_OPEN, -2, 2, 0, GK_WHEEL, "[Well,|Hah.|Heh.|Now,|Whoa.|(spins a chip)|Alright,]"),
LN(LY, I_OPEN, -2, -2, 0, "[Tarnation.|Dang it.|Now hold on.]"),
LN(LY, I_TAG, -2, 2, 0, "[Partner.|Yessir.|Bets down.|Heh.]"),
LK(LY, I_TAG, -2, -1, 0, GK_WHEEL, "[...Hmph.|Next spin.]"),
LN(LY, I_AFTER, -2, 2, 0, "[Heh.|...] [Don't tell the Moth I said that.|Anyway.]"),
LN(LY, I_RESUME, -2, 2, 0, "[Anyhow.|Where was I.|So like I was sayin'.]"),

/* ================================================================= */
/* THUNDER CROW (slots): a huge, calm, superstitious attendant        */
/* ================================================================= */
LK(CR, I_HELLO, -2, 2, F_FIRST, GK_SLOT, "[I am Thunder Crow.|Sit, little one.] [I look after the machines.|These machines and I go back a long way.] [Choose one. Or let it choose you.]"),
LK(CR, I_HELLO, -2, 2, F_BACK, GK_SLOT, "[You return.|Ah. The little one again.|I remember you.] [Sit. Pull.|The machines are warm {tod}.]"),
LK(CR, I_HELLO, -2, 2, F_BACK | F_RELHI, GK_SLOT, "[You. The one who empties my machines.|The machines grumble when you come in.] [Sit. They are ready for you this time.]"),
LK(CR, I_HELLO, -2, 2, F_BACK | F_RELLO, GK_SLOT, "[Welcome back, little one.|The machines are glad to see you.] [You feed them so well.]"),
LK(CR, I_HELLO, -2, 2, F_RETURN, GK_SLOT, "[Back so soon.|The stool is still warm.] [Sit.]"),
LN(CR, I_HELLO, -2, 2, F_GUP, "[I hear you beat {other}.|{other} lost {gnet} to you.] [Not here. Not to me.]"),
LK(CR, I_HELLO, -2, 2, F_GDOWN, GK_SLOT, "[{other} took {gnet} from you.] [The machines will be gentler. Maybe.]"),
LK(CR, I_WIN, -2, 2, 0, GK_SLOT, "[Hmph.|A good pull.|The machine gives.] [Fortune smiles on you. For now.|It liked that pull.]"),
LK(CR, I_WIN, -2, -1, 0, GK_SLOT, "[Enough{A}.|Again.] [You take too much.|This machine will need a rest after you.|I will have words with this machine.]"),
LK(CR, I_WIN, -2, 2, F_SPECIAL, GK_SLOT, "[Seven. Seven. Seven.|The machine sings.|Three sevens.] [In twenty years I have seen this few times{A}.|Take it. Quickly. Before it changes its mind.]"),
LK(CR, I_WIN, -2, 2, F_HUGE, GK_SLOT, "[{what}. A heavy pull.|The coins pour out.] [The machine is surprised. So am I.]"),
LK(CR, I_WIN, -2, 2, F_MINOR, GK_SLOT, "[A few coins.|Small change.|{what}.] [Better than nothing{A}.|Crumbs. But warm crumbs.]"),
LK(CR, I_WIN, -2, 2, F_COMEBACK, GK_SLOT, "[At last.|The machine gives something back.] [Do not get used to it{A}.]"),
LK(CR, I_LOSE, -2, 2, 0, GK_SLOT, "[Nothing.|Not this pull.|The reels say no.] [The machine is thinking.|It is patient. Be patient too.]"),
LK(CR, I_LOSE, 1, 2, 0, GK_SLOT, "[Pull again.|The machine is hungry {tod}.] [It is good for the machine. Not for you.]"),
LK(CR, I_LOSE, -2, 2, F_NEAR, GK_SLOT, "[Two {sym}. So close.|Almost.|The third one slipped past.] [Almost is the machine's favourite word{A}.|That is how it keeps you.]"),
LK(CR, I_LOSE, -2, 2, F_DSTREAK, GK_SLOT, "[The machine is cold on you {tod}.|Keep feeding it{A}.|Many pulls, little back.] [Try the one by the pillar.|Some machines just do not like some people.]"),
LK(CR, I_IDLE, -2, 2, 0, GK_SLOT, "[Listen.|Hush.] [Hear the reels humming?|Every machine sounds different.]"),
LK(CR, I_IDLE, -2, 2, 0, GK_SLOT, "[Someone left a lucky coin on this machine.|There is a coin taped under this machine.] [Don't touch it.]"),
LK(CR, I_IDLE, -2, 2, 0, GK_SLOT, "[Pull the lever{A}.] [The machine is waiting.]"),
LK(CR, I_IDLE, -2, 2, 0, GK_SLOT, "[I have seen a thousand players at this machine.] [I remember none.|You may be the first I remember.]"),
LK(CR, I_IDLE, -2, 2, F_LONG, GK_SLOT, "[You are very still.|The machine waits.] [I am patient. Mountains are patient.]"),
LK(CR, I_IDLE, -2, 2, F_LATE, GK_SLOT, "[It is very late.|The night crowd is here.] [The machines like the night crowd.]"),
LN(CR, I_OPEN, -2, 2, 0, "[Hm.|Hmph.|Listen.|Little one.]"),
LK(CR, I_OPEN, -2, -2, 0, GK_SLOT, "[Enough.|(bangs the side of the machine)]"),
LK(CR, I_TAG, -2, 2, 0, GK_SLOT, "[Pull.|So it is.|Hm.]"),
LK(CR, I_AFTER, -2, 2, 0, GK_SLOT, "[...The machines agree.|(pats the machine)]"),
LN(CR, I_RESUME, -2, 2, 0, "[Where was I.|As I said.]"),

/* ================================================================= */
/* MIRROR SQUID (video poker): chatty, does impressions of everyone   */
/* ================================================================= */
LN(SQ, I_HELLO, -2, 2, F_FIRST, "[Hi {N}! It's me, your old friend!|Oh! Hi! It's me!] [...You don't remember me? Strange.|From school? No? Work? The dentist?]"),
LK(SQ, I_HELLO, -2, 2, F_FIRST, GK_VP, "[Hello, hello!|Welcome, welcome!] [I'm Mirror Squid. I do video poker and impressions.|Mirror Squid. I can sound like anyone in this building.]"),
LN(SQ, I_HELLO, -2, 2, F_BACK, "[{N}! My favourite regular!|Welcome back! I've been practising your voice!] [Want to hear it? Later.|It's very good. You'll hate it.]"),
LN(SQ, I_HELLO, -2, 2, F_BACK | F_RELHI, "[Oh, it's you.|The card shark!] [I tried playing like you once. I couldn't keep up.]"),
LN(SQ, I_HELLO, -2, 2, F_BACK | F_RELLO, "[My favourite customer!] [I'd hug you, but the Moth says no hugging.]"),
LN(SQ, I_HELLO, -2, 2, F_RETURN, "[You're back! Did you miss me?|Back again? I missed you.] [I missed you more. Probably.]"),
LN(SQ, I_HELLO, -2, 2, F_GUP, "[I heard you beat {other}!|You took {gnet} off {other}? Amazing.] [I do a great {other} impression. Want to hear them cry?]"),
LN(SQ, I_HELLO, -2, 2, F_GDOWN, "[Oof, {other} got you for {gnet}, huh?|Heard about {other}.] [Want me to pretend to be someone who cares? I'm very good.]"),
LN(SQ, I_WIN, -2, 2, 0, "[You win!|Nice!|Ooh.] [Or did I let you? I forget.|Well played, is what your mum would say. So I'm saying it. In her voice.|Good cards. I should know. I dealt them.]"),
LK(SQ, I_WIN, -2, 2, F_SPECIAL, GK_VP, "[{hand}!|Look at that, {hand}{A}.|Oh, {hand}.] [Very nice. I'd clap, but I'm holding the deck.|Nobody tell the Moth.]"),
LK(SQ, I_WIN, -2, 2, F_HUGE, GK_VP, "[{hand}?! Are you kidding me?|Wait. Wait. {hand}!] [I need to sit down.|I'm telling everyone I taught you that.]"),
LK(SQ, I_WIN, -2, 2, F_MINOR, GK_VP, "[{hand}. Your money back, basically.|{hand}. It's something!] [Like a hug from a stranger.|Small wins count. I count them.]"),
LN(SQ, I_WIN, -2, -2, 0, "[Who are you{A}?|Nobody wins like this!|Stop it!] [Are you doing an impression of a winner? Because it's too good.|I'm going to get in trouble!]"),
LN(SQ, I_LOSE, -2, 2, 0, "[Oh dear.|Nothing.|Aw.] [Would you like me to be someone who cares?|That hand didn't suit you.|You lose. I'm told it builds character.]"),
LN(SQ, I_LOSE, 1, 2, 0, "[Unlucky! Says the voice of your mother.|Aww.] [Very convincing, no?|Another one for the house.]"),
LN(SQ, I_LOSE, -2, 2, F_DSTREAK, "[Don't worry, the next hand is the one.|Another loss? Want me to do a sad voice?|You look defeated. Can I borrow that look? It's very you.]"),
LK(SQ, I_PUSH, -2, 2, 0, GK_VP, "[{hand}. Money back!|Even! Like a mirror.] [See? I'm fair. Sometimes.]"),
LK(SQ, I_IDLE, -2, 2, 0, GK_VP, "[Hold the good cards.|Psst.] [Phantom Moth can't hear us. ...Or can he.]"),
LN(SQ, I_IDLE, -2, 2, 0, "[Want to hear my Lynx?|I do a great Crow.] [(deep voice) Pull. The machine is waiting. ...Uncanny, right?|(drawls) Well now, partner. ...He hates it.]"),
LN(SQ, I_IDLE, -2, 2, 0, "[I once did the Moth's voice on the speakers.] [Told everyone drinks were free. Best night ever. Worst morning ever.]"),
LK(SQ, I_IDLE, -2, 2, 0, GK_VP, "[Fun fact.|Did you know?] [Royal flushes come up about once every forty thousand hands.|I've dealt three. Nobody believes me.]"),
LN(SQ, I_IDLE, -2, 2, F_LONG, "[Hello? Are you still there?|Did you fall asleep?] [I can do snoring. Want to hear?]"),
LN(SQ, I_IDLE, -2, 2, F_LATE, "[It's so late!|Past midnight{A}.] [I need my beauty sleep. So do you, honestly.]"),
LN(SQ, I_OPEN, -2, 2, 0, "[Ooh,|Oh!|Okay,|Psst,|Hey,|Heh,]"),
LN(SQ, I_OPEN, -2, -2, 0, "[Ugh!|Hmph!]"),
LN(SQ, I_TAG, -2, 2, 0, "[Right?|Don't tell anyone.|Ha!|...Probably.|Or not!]"),
LN(SQ, I_AFTER, -2, 2, 0, "[Wait, was that rude?|Sorry, that was my Viper voice.|Did I say that out loud?]"),
LN(SQ, I_RESUME, -2, 2, 0, "[Anyway!|Oh, right, where was I.|So!]"),

/* ================================================================= */
/* SILENT VIXEN (Hi-Lo): few words, misses nothing                    */
/* ================================================================= */
LK(VX, I_HELLO, -2, 2, F_FIRST, GK_HILO, "[Sit.|Silent Vixen.] [Higher or lower. That's all.|I deal. You guess. Don't overthink it.]"),
LK(VX, I_HELLO, -2, 2, F_FIRST, GK_HILO, "[Evening.|Hm.] [Know when to stop. That's the only advice I give.]"),
LN(VX, I_HELLO, -2, 2, F_BACK, "[You again.|Back.] [Good. Sit.]"),
LN(VX, I_HELLO, -2, 2, F_BACK | F_RELHI, "[You.] [You read these well. I noticed.]"),
LK(VX, I_HELLO, -2, 2, F_BACK | F_RELLO, GK_HILO, "[You again.] [Try stopping sooner this time.]"),
LN(VX, I_HELLO, -2, 2, F_RETURN, "[Back already.] [Sit.]"),
LN(VX, I_HELLO, -2, 2, F_GUP, "[You beat {other}.|{other} lost {gnet} to you.] [I'm not {other}.]"),
LN(VX, I_HELLO, -2, 2, F_GDOWN, "[{other} took {gnet} from you.] [Sloppy.]"),
LK(VX, I_WIN, -2, 2, 0, GK_HILO, "[Good.|Well called.|You took it.] [You knew when to stop.|Most don't.]"),
LK(VX, I_WIN, -2, 2, F_LONG, GK_HILO, "[{run} in a row{A}.|{run} right.] [Impressive. I mean that.|Few have that nerve.]"),
LN(VX, I_WIN, -2, -1, 0, "[How are you seeing these?|Again.] [Hm. You're good at this.|I'm watching you now.]"),
LK(VX, I_LOSE, -2, 2, 0, GK_HILO, "[Wrong.|No.|Down.] [It happens.|One bad guess.]"),
LK(VX, I_LOSE, -2, 2, F_NEAR, GK_HILO, "[{run} good calls. Then that.|So close to cashing.] [You should have stopped.|One too many.]"),
LK(VX, I_LOSE, -2, 2, 0, GK_HILO, "[{card}.] [You didn't see it coming.|I did.]"),
LN(VX, I_LOSE, -2, 2, F_DSTREAK, "[Another.|Cold streak.] [Take a break. Or don't.]"),
LN(VX, I_PUSH, -2, 2, 0, "[Even.] [Nothing gained.]"),
LN(VX, I_IDLE, -2, 2, 0, "[Take your time.|No rush.]"),
LK(VX, I_IDLE, -2, 2, 0, GK_HILO, "[Middle cards are the hard ones.] [Sevens. Eights.]"),
LK(VX, I_IDLE, -2, 2, 0, GK_HILO, "[The next card doesn't care what the last one was.]"),
LN(VX, I_IDLE, -2, 2, F_LONG, "[You're patient.] [Good.]"),
LN(VX, I_IDLE, -2, 2, F_LATE, "[Late.] [The quiet hours. My favourite.]"),
LN(VX, I_OPEN, -2, 2, 0, "[Hm.|...]"),
LN(VX, I_OPEN, -2, -2, 0, "[Tch.]"),
LN(VX, I_TAG, -2, 2, 0, "[Again.|Go on.]"),
LN(VX, I_RESUME, -2, 2, 0, "[...|Where was I.]"),

/* ================================================================= */
/* CRIMSON VIPER (baccarat, the Suite): proud, wants the top office   */
/* ================================================================= */
LN(VP, I_HELLO, -2, 2, F_FIRST, "[Welcome to the top{A}.|So you're the one the Moth keeps talking about.] [Only the best of us make it up here.|We'll see.]"),
LK(VP, I_HELLO, -2, 2, F_FIRST, GK_BACC, "[Baccarat.] [A game for kings. Or for people who pretend to be.]"),
LN(VP, I_HELLO, -2, 2, F_BACK, "[Back in my Suite{A}.|You again, brother.] [I hope you brought more than last time.|Sit. Try not to touch anything.]"),
LN(VP, I_HELLO, -2, 2, F_BACK | F_RELHI, "[You.] [I've thought about you every night. Tonight, you lose.]"),
LN(VP, I_HELLO, -2, 2, F_BACK | F_RELLO, "[Ah, my loyal donor.] [The Suite thanks you{A}.]"),
LN(VP, I_HELLO, -2, 2, F_RETURN, "[Back so soon, brother?|You missed the view.] [Sit.]"),
LN(VP, I_HELLO, -2, 2, F_GUP, "[I hear you took {gnet} off {other}.|{other} lost to you. Of course.] [{other} is beneath me. You'll find I'm not.]"),
LN(VP, I_HELLO, -2, 2, F_GDOWN, "[{other} took {gnet} from you downstairs.] [Pathetic. That was the easy table.]"),
LN(VP, I_WIN, -2, 2, 0, "[Enjoy it, brother.|Hmph.|Fine.] [It won't happen twice.|Not bad for someone from the Main Floor.|Take it. I have plenty.]"),
LK(VP, I_WIN, -2, 2, F_TIE, GK_BACC, "[A tie. And you backed it.|Eight to one.] [Either genius or madness{A}.]"),
LK(VP, I_WIN, -2, 2, F_SPECIAL, GK_BACC, "[A natural {wv}.|Natural {wv}.] [The cards bow to you{A}. For now.]"),
LK(VP, I_WIN, -2, 2, F_CLOSE, GK_BACC, "[{wv} to {lv}.|By one point.] [Barely, brother. Barely.]"),
LN(VP, I_WIN, -2, -2, 0, "[Enough!|You don't belong up here!|Is the Moth feeding you cards?] [I knew it!|You think you're better than me?!]"),
LN(VP, I_LOSE, -2, 2, 0, "[Did you really think you could beat me up here?|This is my table.|The Suite eats people like you.] [You're just visiting{A}.|Down you go.]"),
LN(VP, I_LOSE, -2, 2, F_DSTREAK, "[Is that all{A}?|Keep losing.|Again?] [I expected more.|I'm enjoying the view.]"),
LK(VP, I_LOSE, -2, 2, F_CLOSE, GK_BACC, "[{wv} to {lv}.|One point.] [So close, brother. So far.]"),
LK(VP, I_LOSE, -2, 2, F_SPECIAL, GK_BACC, "[A natural. {wv}.|Natural {wv}. Not yours.] [That's how it's done up here.]"),
LK(VP, I_PUSH, -2, 2, 0, GK_BACC, "[A tie.|Even.] [The cards couldn't decide. I can.]"),
LN(VP, I_IDLE, -2, 2, 0, "[I was made for this room.] [You were made to visit it.]"),
LN(VP, I_IDLE, -2, 2, 0, "[The Moth reads minds.] [I read money. Guess who runs the Suite.]"),
LN(VP, I_IDLE, -2, 2, 0, "[Look around.] [Every chip up here answers to me.]"),
LK(VP, I_IDLE, -2, 2, 0, GK_BACC, "[Pick a side{A}.] [Player or Banker. Me or you.]"),
LK(VP, I_IDLE, -2, 2, F_LONG, GK_BACC, "[Take your time, brother.] [Kings are patient. Until they aren't.]"),
LN(VP, I_IDLE, -2, 2, F_RICH, "[Now that's a stack worthy of the Suite.] [Almost.]"),
LN(VP, I_OPEN, -2, 2, 0, "[Hmph.|Listen,|Brother,|Heh.|(swirls a glass)]"),
LN(VP, I_OPEN, -2, -2, 0, "[Grr.|Damn it.]"),
LN(VP, I_TAG, -2, 2, 0, "[Brother.|Remember that.]"),
LN(VP, I_TAG, -2, -1, 0, "[...This isn't over.]"),
LN(VP, I_AFTER, -2, 2, 0, "[Remember that{A}.|...Brother.]"),
LN(VP, I_RESUME, -2, 2, 0, "[Anyway.|As I was saying.]"),

#include "talk_table.inc"

/* ================================================================= */
/* anyone                                                             */
/* ================================================================= */
LN(AN, I_MEM, -2, 2, M_PEAK, "[You were up {mem} earlier{A}.|Remember when you had {mem}?] [Where did it all go?|Good times.]"),
LN(AN, I_MEM, -2, 2, M_HANDS, "[That's {hands} rounds{A}.|{hands} rounds with me.] [You're a regular now.|I know your habits now.]"),
LN(AN, I_MEM, -2, 2, M_BEST, "[That {mem} win earlier{A}.|Still thinking about that {mem} win?] [It won't happen again.|I am.]"),
LN(AN, I_HELLO, -2, 2, 0, "[Evening{A}.|Welcome{A}.|Hello again{A}.] [Take a seat.|Good to see you.]"),
LN(AN, I_QWIN, -2, 2, 0, "[Well played.|You win.|Nice.|Good one.|Enjoy it.]"),
LN(AN, I_QLOSE, -2, 2, 0, "[The house wins.|Unlucky.|Better luck next time.|Not this time.]"),
LN(AN, I_QPUSH, -2, 2, 0, "[A push.|Even.|Bet back.]"),
LN(AN, I_RESUME, -2, 2, 0, "[Anyway.|Where was I.]"),
};
#undef MO
#undef LY
#undef CR
#undef SQ
#undef VX
#undef VP
#undef AN
