cdb -pv %1 %2 -logo out.txt -lines -c "!sym prompts;.reload;.cxr dwo(%3+4);kb1000;!for_each_frame dv /t;q"