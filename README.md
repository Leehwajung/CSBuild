#명세

- 사용자는 "컴파일 플래그를 검색 프로그램"을 사용하여, 빌드에서 사용되는 컴파일 플래그를 가져올 수 있다.
- 사용자는 프로그램을 프로그램을 실행시키고, 이 프로그램을 실행하는 중에 C/C++ 프로그램을 빌드한다.
    -  비주얼 스튜디오로 빌드할 수 있다.
    -  GNU 컴파일러로 빌드할 수 있다.
- 빌드를 수행하는 프로세스의 PID를 이용해서 커맨드 라인을 가져온다.
- 가져온 커맨드라인의 정보에 CL.EXE가 포함되어 있다면, 컴파일 플래그를 읽어온다.
- 가져온 커맨드라인의 정보에 LINK.EXE가 포함되어 있다면, 링크 플래그를 읽어온다.
- 가져온 커맨드라인의 정보에 GCC.EXE 또는(OR) G++.EXE가 포함되어 있다면, 컴파일 플래그와 링크플래그를 읽어온다.
- 추출한 빌드 정보를 XML 파일 형식으로 출력한다.

