# 토너먼트(*.TRN) 문구: 대회 이름·설명·우승 엔딩·상대 선수 이름과 대사.
# TRN 파일은 그대로 두고, 화면에 그릴 때 KOREAN.UI 로 바꾼다 (make_ui.py 가 합침). "\n" 은 줄바꿈
UI = {
# 대회 이름
'North American Open': '북미 오픈',
'Katushai Challenge': '카투샤이 챌린지',
'WAR Invitational': 'WAR 초청전',
'World Championship': '세계 선수권',

# 대회 설명
'Attempt to wrestle the North American title from the two time world champion, Raven.  The north American open attracts respected competitors from around the globe.\\n\\nWinnings: moderate\\nCompetitors: Intermediate\\nREGISTRATION FEE: 1,500k':
'세계 챔피언 2회의 레이븐에게서 북미 타이틀을 빼앗아 오십시오. 북미 오픈에는 세계 곳곳의 이름난 선수들이 모입니다.\\n\\n상금: 보통\\n상대: 중급\\n참가비: 1,500k',
'This militant country has managed, in the past years, to establish a respected tournament.  The competitors are typically well trained and equally well financed.\\n\\nWinnings: Good\\nCompetitors: Skilled\\nREGISTRATION FEE: 3,000k':
'호전적인 이 나라는 지난 몇 년 동안 권위 있는 대회를 키워 냈습니다. 선수들은 대체로 훈련이 잘되어 있고 자금도 넉넉합니다.\\n\\n상금: 좋음\\n상대: 숙련\\n참가비: 3,000k',
'The WAR invitational attracts the finest of the worlds fighters.  Not only does this tournament bring spectators from around the country, but the prestige of winning this title is second only to being world champion.\\n\\nWinnings: EXCELLENT\\nCompetitors: ADVANCED\\nREGISTRATION FEE: 5,000k\\n':
'WAR 초청전에는 세계 최고의 선수들이 모입니다. 전국에서 관중이 몰려들 뿐 아니라, 이 타이틀의 명예는 세계 챔피언 다음으로 높습니다.\\n\\n상금: 아주 좋음\\n상대: 상급\\n참가비: 5,000k\\n',
'If you think you are ready to take on the world, here\'s your chance.  The world championship assembles the greatest fighters from around the globe.  Since millions of spectators attend the fights, the pay is excellent.  Fight hard and train hard, and one day the world will call you their champion.\\n\\nWinnings: Outstanding\\nCompetitors: World-Class\\nREGISTRATION FEE: 10,000k':
'세계와 맞설 준비가 되었다면 지금이 기회입니다. 세계 선수권에는 전 세계 최강의 선수들이 모입니다. 수백만 관중이 경기를 보러 오므로 상금도 아주 큽니다. 열심히 싸우고 열심히 훈련하면, 언젠가 세계가 당신을 챔피언이라 부를 것입니다.\\n\\n상금: 최고\\n상대: 월드 클래스\\n참가비: 10,000k',

# 우승 엔딩
'As the hanger door closes on your robot, you can\'t stop thinking about the fight.  Your heart races and your hands shake with adrenaline; you feel invulnerable.':
'격납고 문이 로봇 뒤로 닫히는데도 경기 생각이 멈추지 않는다. 심장이 뛰고 아드레날린에 손이 떨린다. 무엇에도 지지 않을 것만 같다.',
'The fight was hard, but you made it.  You are the North American Champion.':
'힘든 싸움이었지만 해냈다. 당신은 북미 챔피언이다.',
'You find it hard to walk, hard to breathe, hard to do anything but think about the next fight... the next challenge...':
'걷기도, 숨쉬기도 힘들다. 다음 경기... 다음 도전 말고는 아무것도 생각할 수 없다...',
'Your next chance to prove that you are truely the greatest the world has ever seen.':
'당신이 세상에 다시없는 최고임을 증명할 다음 기회만을.',
'A cheering crowd runs toward you.  They stop immediately when you fire off your thrusters and rise above them.':
'환호하는 관중이 달려온다. 당신이 추진기를 켜고 그들 위로 떠오르자 모두 그 자리에 멈춰 선다.',
'A cheering crowd scurries below as you tower above your battered and beaten foe.':
'만신창이가 된 적 위에 우뚝 선 당신의 발아래로 환호하는 관중이 몰려든다.',
'A crowd of ecstatic spectators approaches your jaguar as you raise a hand in victory.':
'당신이 승리의 손을 들어 올리자 열광한 관중이 재규어로 다가온다.',
'You are a hero to the people of Katushai, for now you are their undisputed champion.':
'이제 당신은 누구도 부정할 수 없는 챔피언, 카투샤이 사람들의 영웅이다.',
'High atop WAR\'s headquarters flies the hundred meter long victory banner.':
'WAR 본부 꼭대기에 길이 백 미터의 승리 깃발이 휘날린다.',
'The banner glorifies WAR\'s finest fighter; the combatant with the desire and the power to defeat the world\'s finest.':
'그 깃발은 WAR 최고의 선수, 세계 최강을 꺾을 의지와 힘을 가진 전사를 기린다.',
'Today the banner celebrates a new champion, a competitor who rose through the ranks with outstanding skill and a hunger to overcome.':
'오늘 그 깃발은 뛰어난 실력과 이기려는 갈망으로 정상에 오른 새 챔피언을 축하한다.',
'Today the banner celebrates you.':
'오늘 그 깃발은 당신을 축하한다.',
'After defeating the greatest fighters in the world, you finally have a chance to relax.':
'세계 최강의 선수들을 물리치고, 마침내 쉴 틈이 생겼다.',
'As the storm rages overhead, you feel completely calm, content to know that you have triumphed over the best this country has to offer.':
'머리 위로 폭풍이 몰아쳐도 마음은 고요하다. 이 나라 최고의 선수들을 이겼다는 것으로 충분하다.',
'As you stare at the holographic world championship trophy, you can\'t help thinking about the other fighters...':
'홀로그램 세계 선수권 트로피를 바라보며, 다른 선수들을 떠올리지 않을 수 없다...',
'A smile crosses your face as you remember the cheering crowds, wildly applauding as you accepted the trophy.':
'트로피를 받을 때 열광적으로 박수 치던 관중을 떠올리자 얼굴에 미소가 번진다.',
'The look on their face... The words on their lips... They all said the same thing...':
'그들의 표정... 그들의 입술에 맺힌 말... 모두 같은 말을 하고 있었다...',
'"See you next year."':
'"내년에 또 보자."',

# 상대 선수 이름 (스토리 파일럿과 같은 표기)
'Angel': '엔젤', 'Ariel': '에어리얼', 'Bethany': '베서니', 'Bruce': '브루스', 'Christian': '크리스티안',
'Cossette': '코세트', 'Crystal': '크리스탈', 'Devan Shell': '데반 셸', 'Dr. Lynn Yarr': '린 야르 박사',
'Eva Earlong': '에바 얼롱', 'Eva O\'Ryan': '에바 오라이언', 'ICEMAN': '아이스맨', 'Ian Tavares': '이언 타바레스',
'Ibrahim': '이브라힘', 'Jack': '잭', 'Jacob': '제이콥', 'Jahrod': '재로드', 'James': '제임스',
'Jaqouline': '재클린', 'Jazzy': '재지', 'Jean Paul': '장폴', 'Killian': '킬리언', 'Marissa': '마리사',
'Milano': '밀라노', 'Nathaniel': '너새니얼', 'Nicoli': '니콜리', 'Prince Vassar': '바사르 왕자',
'Raven': '레이븐', 'Rolland': '롤랜드', 'Scarlet': '스칼렛', 'Selenna': '셀레나', 'Shirro': '시로',
'Steel Claw': '스틸 클로', 'Steffan': '스테판', 'Veronica': '베로니카',

# 상대 선수 대사 (경기 전 화면)
'Ain\'t you quit yet?': '아직도 안 그만뒀냐?',
'Alright!  A challenge at last!': '좋아! 드디어 상대다운 상대군!',
'And now, I will send you back where you came from.': '이제 네가 온 곳으로 돌려보내 주지.',
'Are sure you should fight tonight?  You don\'t look well.': '오늘 밤 정말 싸울 거야? 안색이 안 좋은데.',
'Buy ya some holics after the match?  After all, I\'ll be alot richer!': '경기 끝나고 한잔 살까? 어차피 난 훨씬 부자가 될 테니!',
'Can\'t wait to shove this fist through your chest!': '이 주먹으로 네 가슴을 꿰뚫고 싶어 못 참겠군!',
'Don\'t be scared, honey.  I\'ll be gentle.': '겁먹지 마, 자기. 살살 해 줄게.',
'Don\'t let our last fight cloud your head.  The victory will belong to me.': '지난번 싸움은 잊어. 승리는 내 것이 될 테니.',
'Don\'t worry, this won\'t hurt much...': '걱정 마, 별로 안 아플 거야...',
'Don\'t you think that \'bot was a bad choice for you?': '그 로봇은 잘못 고른 것 같지 않아?',
'Haven\'t you learned from the others that I\'m the wrong lady to cross?': '나한테 덤비면 안 된다는 걸 다른 녀석들 보고도 못 배웠어?',
'Hello again, pretty thang!  Gonna getcha this time!': '또 만났네, 예쁜이! 이번엔 꼭 잡는다!',
'How dare you treat my sister like that?  Your cruelty will cost you!': '감히 내 누이를 그렇게 대해? 그 잔인함의 대가를 치르게 해 주지!',
'I am the smartest, fastest, and best looking fighter here!  A natural!': '여기서 제일 똑똑하고, 제일 빠르고, 제일 잘생긴 선수는 나야! 타고났지!',
'I came here to eat carrots and kick butt, and I\'m all out of carrots!': '당근 먹고 혼내 주러 왔는데, 당근이 다 떨어졌거든!',
'I cannot allow you to treat a lady that way!  Prepare to meet your maker.': '숙녀를 그렇게 대하는 건 용서 못 해! 저세상 갈 준비나 해라.',
'I defeat you.  I become next world champion.  This tournament just next step.': '나 너 이긴다. 나 다음 세계 챔피언 된다. 이 대회 그냥 다음 단계.',
'I don\'t even want the money.  Just the thrill of seeing your face after I stomp on it.': '돈도 필요 없어. 네 얼굴을 짓밟은 뒤에 보는 그 짜릿함이면 돼.',
'I don\'t think you\'ll last much longer...': '넌 오래 못 버틸 것 같은데...',
'I have a few surprises in store for you!': '널 위해 깜짝 선물을 몇 개 준비했지!',
'I have learned much since our last encounter.  I believe you will be defeated.': '지난번 만남 이후 많은 것을 배웠다. 너는 패배할 것이다.',
'I have studied you.  I find you an adequate challenge.': '너를 분석했다. 상대할 만한 수준이군.',
'I hope you aren\'t planning on celebrating tonight.': '오늘 밤 축하할 생각은 안 했길 바라.',
'I may be young, but I was trained by the best!': '어려도 최고한테 배웠다고!',
'I saw what you did to my woman!  Let me show you what speed is all about!': '네가 내 여자한테 한 짓 다 봤어! 진짜 빠르다는 게 뭔지 보여 주지!',
'I see you\'ve stumbled past the grunts,  but you\'re going to have to dance with fire to beat me.': '졸개들은 어찌어찌 넘었나 본데, 날 이기려면 불길 속에서 춤춰야 할 거다.',
'I was born for this game!  You were obviously ill-chosen for it.': '난 이 경기를 위해 태어났어! 넌 확실히 잘못 뽑혔고.',
'I will stop your clock.': '네 시계를 멈춰 주마.',
'I\'ll be surprised if you make it past 30 ticks.': '30초만 버텨도 놀랍겠는걸.',
'I\'ll wrap a chain around your neck till you say "uncle"!': '"항복"이라고 할 때까지 목에 사슬을 감아 주지!',
'I\'m afraid you haven\'t spent hardly enough time in the sims, little one.': '꼬마야, 시뮬레이터에서 보낸 시간이 턱없이 모자란 것 같구나.',
'I\'m going to light you up, old friend.': '불태워 주지, 옛 친구.',
'I\'ve been waiting to add a few dents to that little \'bot of yours.': '네 그 조그만 로봇을 좀 찌그러뜨려 주고 싶었거든.',
'I\'ve been watching you.  Can\'t wait to cut you in half...': '널 지켜봤어. 반으로 갈라 주고 싶어 못 참겠군...',
'I\'ve seen you on replay.  Compared to me, you fight in slow motion!': '네 경기 다시보기로 봤어. 나에 비하면 슬로 모션이더군!',
'In my country, we would have never even let you sign up!': '우리나라였으면 넌 참가 신청도 못 했을 거다!',
'It\'s time I show your kind a thing or two about pain!': '너 같은 놈들한테 고통이 뭔지 좀 가르쳐 줄 때가 됐군!',
'Just listen to those fans.  You know, you can quit now and save the embarrassment.': '저 팬들 소리 좀 들어 봐. 지금 그만두면 망신은 면할 수 있어.',
'My diagnosis is you\'re about to have a throbbing headache.': '진단 결과, 곧 머리가 지끈지끈 아파질 겁니다.',
'No problem.  One more notch on the helmet.': '문제없지. 헬멧에 눈금 하나 더 새기면 돼.',
'Now HERE\'S a machine I can really rock with!': '바로 이거지, 제대로 놀 수 있는 기계야!',
'Now watch what I can do with THIS robot!!': '이 로봇으로 내가 뭘 할 수 있는지 봐라!!',
'Once I take to the air, you might as well not even be in the arena!': '내가 날아오르면 넌 경기장에 없는 거나 마찬가지야!',
'Please be gentle, I\'m kinda new at this...': '살살 해 줘, 나 이거 처음이라...',
'Prepare to meet your maker.': '저세상 갈 준비나 해라.',
'Slice \'bot you got there.  Mind if I take a few whacks at it?': '로봇 멋진데. 몇 대 쳐 봐도 될까?',
'Smart money\'s on me, grunt.  Better just hang back.': '돈 좀 아는 사람들은 나한테 건다, 졸병. 뒤로 빠져 있어.',
'So did you enjoy your last win?': '그래, 지난번 승리는 즐거웠나?',
'So you made it this far, eh?  Can\'t wait to grab another victory from you!': '여기까지 왔다 이거지? 너한테서 또 한 번 승리를 가져가 주마!',
'So, did you enjoy your last win?': '그래, 지난번 승리는 즐거웠나?',
'Sorry, but I don\'t give autographs...  Oh, you think you\'re good enough to FIGHT me?': '미안하지만 사인은 안 해 줘... 어, 나랑 싸울 실력은 된다고 생각하는 거야?',
'The championship is rightfully mine!  I will show the world you are not a worthy champion!': '챔피언 자리는 당연히 내 거다! 네가 챔피언감이 아니라는 걸 세상에 보여 주지!',
'This is where your career ends.': '네 선수 생활은 여기서 끝이다.',
'Thought you\'d seen the last of me, eh?': '다시는 날 안 볼 줄 알았지?',
'Tonight just isn\'t going to be your night, pal.': '오늘 밤은 네 날이 아니야, 친구.',
'Tonight the audience won\'t be the only one shocked.': '오늘 밤 충격받는 건 관중만이 아닐 거다.',
'Wanna make some money quick?  Bet on me!': '돈 좀 빨리 벌고 싶어? 나한테 걸어!',
'Watch me fly!': '내가 나는 걸 봐라!',
'Wave good-bye to the fans, now...': '이제 팬들한테 작별 인사나 해...',
'Welcome to NorthAm.  You had better be prepared.': '북미에 온 걸 환영한다. 각오는 돼 있겠지.',
'Well look at little ole you.  Made it alot farther than I thought you would!': '어머, 꼬맹이 좀 봐. 생각보다 훨씬 멀리 왔네!',
'Well, your a good fighter, and kinda cute, too!': '음, 실력도 좋고 좀 귀엽기도 하네!',
'Why do they keep making me fight such weaklings?': '왜 자꾸 이런 약골이랑 싸우게 하는 거야?',
'With my new adrenaline pump, I\'m the best money can buy!!': '새 아드레날린 펌프를 단 나는 돈으로 살 수 있는 최고라고!!',
'Yer stupid to wanna fight me.  This one\'s gonna cost you, punk!': '나랑 싸우겠다니 멍청하군. 비싼 대가를 치르게 될 거다, 애송이!',
'You are inferior.': '너는 열등하다.',
'You can hardly wait.': '그날이 몹시 기다려진다.',
'You got lucky, mate.  Ain\'t gonna happen again!': '운이 좋았지, 친구. 두 번은 없어!',
'You had better hope luck is on your side, \'cause talent is on mine.': '운이 네 편이길 빌어라. 재능은 내 편이니까.',
'You have fought well, but I am the rightful champion.': '잘 싸웠지만, 진짜 챔피언은 나다.',
'You look like you\'ve barely learned how to handle that \'bot!': '그 로봇 다루는 법도 겨우 배운 것 같은데!',
'You may fall before you ever even see me...': '날 만나기도 전에 쓰러질지도 모르지...',
'You purebloods will soon learn the benefit of cyberstructure!': '너희 순수 인간들은 곧 사이버 신체의 장점을 깨닫게 될 거다!',
'You really are a great opponent!  I enjoy our fights...': '넌 정말 멋진 상대야! 너랑 싸우는 게 즐거워...',
'You think YOU\'RE fast?  Compared to me, you\'re a turtle!': '네가 빠르다고? 나에 비하면 거북이야!',
'You\'re in my way, punk!': '비켜, 애송이!',
'You\'ve been lucky to defeat your own people.  You don\'t stand a chance against me!': '네 나라 사람들을 이긴 건 운이었어. 나한텐 상대가 안 돼!',
'Your skill is great, but you will lose.': '실력은 대단하지만, 너는 진다.',
}
