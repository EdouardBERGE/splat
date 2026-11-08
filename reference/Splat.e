-> SPLAT.e v1.0    NeVeZ' file splitter!
-> -------------------------------------
-> 11/4/96 by Stephen Cantini
-> -------------------------------------

MODULE 'dos/dos','dos/rdargs'

ENUM OK,ER_MEM,ER_ARGS,ER_LENGTH,ER_IO,ER_BREAK,ER_INT,ER_UNIMP

CONST EOF=-1

DEF a,z,opts[15]:ARRAY OF LONG,name[1024]:STRING,search[1024]:STRING

PROC main() HANDLE
    DEF rda=NIL:PTR TO rdargs,flag,flen,fhandle=NIL,fout=NIL,size,count,
        er,mem=NIL,buflen,blockleft,eof,eob,offset,lines,sflag,max

    a:='$VER: Splat 1.0 (11.4.96) (c) Copyright Stephen Cantini'

    -> SET extended help STRING

    rda:=AllocDosObject(DOS_RDARGS,NIL)
    IF rda=NIL THEN Raise(ER_MEM)
    rda.exthelp:='Splat V1.0 (c) Stephen Cantini\n\n'+
                 'FILE/A = The file to be splitted\n'+
                 'S=SEARCH/K = String to search for.\nSIZE/N = Split at this number of bytes.\nK/S = Size is expressed in kbytes.\n'+
                 'LINES/S = Size is expressed in lines.\nTO/K = Optional destination directory/filenames (DEFAULT:input file''s).\nR=RECURSIVE/S = Split EVERY size bytes/k/lines/search.\n'+
                 'QUIET/S = Sshhhh!! :)\nBUF=BUFFER/K/N = I/O buffer size in kbytes.\nPARTS/S = Divide in SIZE parts.\nNOCUT/S = Do not trunc text lines.\n'+
                 'QUICK/S = Output JUST the number of files created.\nNOCASE/S = Make string search case insensitive.\nDOWILD/S = Allow jolly character "?" in search string.\n'+
                 'MAX/K/N = Max number of files to create.\n'

    -> Read command arguments

    opts:=[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
    IF ReadArgs('FILE/A,S=SEARCH/K,SIZE/N,K/S,LINES/S,TO/K,R=RECURSIVE/S,QUIET/S,BUF=BUFFER/K/N,PARTS/S,NOCUT/S,QUICK/S,NOCASE/S,DOWILD/S,MAX/K/N',opts,rda)=NIL THEN Raise(ER_ARGS)

    IF CtrlC() THEN Raise(ER_BREAK)

    -> Open input file

    flen:=FileLength(opts[0])
    IF flen<1 THEN Raise(ER_IO)
    fhandle:=Open(opts[0],OLDFILE)
    IF fhandle=NIL THEN Raise(ER_IO)

    -> Parse search STRING FOR \n\t etc, IF any

    IF opts[1] THEN parsestr(opts[1],search)

    IF (opts[1]=NIL) AND (opts[2]=NIL)
        WriteF('Splat: required argument missing\n')
        Raise(OK)
        ENDIF

    size:=0 ; buflen:=0

    -> SET buffer size

    IF opts[8]
        a:=opts[8]
        buflen:=(^a)*1024
        ENDIF

    -> SET block size, IF any

    IF opts[2]
        a:=opts[2]
        size:=^a
        IF opts[3] THEN size:=size*1024
        IF flen<=size THEN Raise(ER_LENGTH)
        IF opts[4]
            lines:=size
            size:=flen
            ENDIF
        ENDIF

    -> IF keyword PARTS exists, calculate file size AND force RECURSIVE mode

    IF opts[9]
        opts[6]:=1
        size:=Div(flen,size)+1
        ENDIF

    IF opts[14]
        max:=opts[14]
        max:=^max
        IF max<2 THEN Raise(ER_ARGS)
        ENDIF

    -> Allocate possibly SIZE bytes (OR specified buffer size), with an 8k buffer fallback

    IF buflen=0
        IF opts[1] THEN buflen:=flen ELSE buflen:=size
        ENDIF

    mem:=AllocMem(buflen,0)
    IF mem=NIL
        mem:=AllocMem(8192,0)
        IF mem=NIL THEN Raise(ER_MEM)
        buflen:=8192
        ENDIF
    IF opts[4] THEN size:=lines

    IF CtrlC() THEN Raise(ER_BREAK)

    -> Read the first buffer

    count:=Read(fhandle,mem,buflen)
    IF count=-1 THEN Raise(ER_IO)
    IF count=0 THEN eof:=TRUE
    offset:=0

    -> Main LOOP

    a:=-1
    eof:=0
    sflag:=NIL
    z:=search
    REPEAT
        IF opts[2] AND (opts[4]=0)
            blockleft:=size
            eob:=0
            REPEAT
                IF (offset+blockleft<=buflen) OR (count<buflen)
                    IF fout=NIL THEN fout:=opennext()
                    IF (count<buflen) AND (count-offset<blockleft)
                        blockleft:=count-offset
                        eof:=TRUE
                        ENDIF
                    er:=Write(fout,mem+offset,blockleft)
                    IF er=-1 THEN Raise(ER_IO)
                    offset:=offset+blockleft
                    eob:=TRUE
                    IF offset=count
                        count:=Read(fhandle,mem,buflen)
                        IF count=-1 THEN Raise(ER_IO)
                        IF count=0 THEN eof:=TRUE
                        offset:=0
                        ENDIF
                ELSE
                    IF fout=NIL THEN fout:=opennext()
                    er:=Write(fout,mem+offset,buflen-offset)
                    IF er=-1 THEN Raise(ER_IO)
                    blockleft:=blockleft-(buflen-offset)
                    count:=Read(fhandle,mem,buflen)
                    IF count=-1 THEN Raise(ER_IO)
                    IF count=0 THEN eof:=TRUE
                    offset:=0
                    ENDIF
                IF CtrlC() THEN Raise(ER_BREAK)
                UNTIL eob OR eof
            IF opts[6]=NIL THEN size:=flen
            DEC max
            IF max=1 THEN size:=flen
        ELSEIF opts[4]
            lines:=size
            REPEAT
                IF offset>=count
                    count:=Read(fhandle,mem,buflen)
                    IF count=-1 THEN Raise(ER_IO)
                    IF count=0 THEN eof:=TRUE
                    offset:=0
                    ENDIF
                IF eof=NIL
                    MOVE.L mem,A0
                    MOVE.L offset,D0
                    MOVE.L lines,D1
w2:                 CMPI.B #10,0(A0,D0.L)
                    BEQ.S  x3
                    CMP.L  count,D0
                    BEQ.S  x2
x4:                 ADDQ.L #1,D0
                    BRA.S  w2
x3:                 SUBQ.L #1,D1
                    BNE.S  x4
x2:                 MOVE.L D0,flag
                    MOVE.L D1,lines
                    IF flag<count THEN INC flag
                    IF CtrlC() THEN Raise(ER_BREAK)
                    IF fout=NIL THEN fout:=opennext()
                    er:=Write(fout,mem+offset,flag-offset)
                    IF er=-1 THEN Raise(ER_IO)
                    offset:=flag
                    ENDIF
                UNTIL (lines=0) OR eof
                IF opts[6]=NIL THEN size:=flen
                DEC max
                IF max=1 THEN size:=flen
            ENDIF
        IF opts[1]
            REPEAT
                IF offset>=count
                    count:=Read(fhandle,mem,buflen)
                    IF count=-1 THEN Raise(ER_IO)
                    IF count=0 THEN eof:=TRUE
                    offset:=0
                    ENDIF
                IF eof=NIL
                    flag:=searchstr(mem,count,offset)
                    IF sflag THEN flag:=count
                    IF flag>count THEN DEC flag
                    IF CtrlC() THEN Raise(ER_BREAK)
                    IF fout=NIL THEN fout:=opennext()
                    er:=Write(fout,mem+offset,flag-offset)
                    IF er=-1 THEN Raise(ER_IO)
                    offset:=flag
                    ENDIF
                UNTIL (offset<count) OR eof
            IF opts[6]=NIL THEN sflag:=TRUE
            DEC max
            IF max=1 THEN sflag:=TRUE
            ENDIF
        REPEAT
            IF offset>=count
                count:=Read(fhandle,mem,buflen)
                IF count=-1 THEN Raise(ER_IO)
                IF count=0 THEN eof:=TRUE
                offset:=0
                ENDIF
            IF opts[10] AND (eof=NIL)
                MOVE.L mem,A0
                MOVE.L offset,D0
w:              CMPI.B #10,0(A0,D0.L)
                BEQ.S  x
                CMP.L  count,D0
                BEQ.S  x
                ADDQ.L #1,D0
                BRA.S  w
x:              MOVE.L D0,flag
                INC flag
                er:=Write(fout,mem+offset,flag-offset)
                IF er=-1 THEN Raise(ER_IO)
                offset:=flag
                ENDIF
            UNTIL (offset<count) OR eof
        Close(fout) ; fout:=NIL
        UNTIL eof
    IF (opts[7]=NIL)
        IF (opts[11]=NIL) THEN WriteF('\d files created.\n',a+1) ELSE WriteF('\d',a+1)
        ENDIF

EXCEPT DO
    IF fout THEN Close(fout)
    IF mem THEN FreeMem(mem,buflen)
    IF fhandle THEN Close(fhandle)
    IF rda THEN FreeArgs(rda)
    IF rda THEN FreeDosObject(DOS_RDARGS,rda)
    IF exception
        SELECT exception
        CASE ER_MEM ; WriteF('Not enough memory.\n')
        CASE ER_ARGS ; PrintFault(IoErr(),'Splat')
        CASE ER_IO ; PrintFault(IoErr(),'Splat')
        CASE ER_UNIMP ; WriteF('Function is not implemented yet.\n')
        CASE ER_LENGTH ; WriteF('SIZE argument is bigger than file size.\n')
        CASE ER_BREAK ; WriteF('***Break\n')
        ENDSELECT
        ENDIF
    CleanUp()
ENDPROC

PROC parsestr(str,b)
    DEF a,ch,x

    a:=0 ; x:=0
    REPEAT
        ch:=str[a++]
        IF (ch="?") AND opts[13] THEN ch:=255
        IF ch="\\"
            ch:=str[a++]
            IF ch="n" THEN b[x++]:=10
            IF ch="\\" THEN b[x++]:="\\"
            IF ch="t" THEN b[x++]:=9
            IF ch="?" THEN b[x++]:="?"
        ELSE
            b[x++]:=ch
            ENDIF
        UNTIL ch=0
    SetStr(b,x)
ENDPROC

PROC opennext()
    DEF ext[5]:STRING,fout=NIL

    INC a
    StrCopy(name,'NULL')
    IF opts[5]
        StrCopy(name,opts[5])
        IF (name[EstrLen(name)-1]="/") OR (name[EstrLen(name)-1]=":") THEN StrAdd(name,FilePart(opts[0]))
    ELSE
        StrCopy(name,opts[0])
    ENDIF

    StringF(ext,'.\d',a)
    StrAdd(name,ext)

    fout:=Open(name,NEWFILE)
    IF fout=NIL THEN Raise(ER_IO)
ENDPROC fout

PROC searchstr(buf,bufsize,offset)
    bufsize:=buf+bufsize
    offset:=buf+offset
    IF opts[12]=NIL
        MOVE.L offset,A1
        MOVE.L bufsize,D2
        MOVEQ  #0,D0
        MOVE.L z,A0
nx:     MOVE.B (A0)+,D0
        BEQ.S  ex
        BPL.S  zz
        ADDQ.L #1,A1
        CMP.L  A1,D2
        BEQ.S  ex
        BRA.S  nx
zz:     CMP.L  A1,D2
        BEQ.S  ex
        CMP.B  (A1)+,D0
        BEQ.S  nx
        MOVE.L search,A0
        BRA.S  nx
ex:     MOVE.L A1,offset
        MOVE.L A0,z
        DEC z
        IF (^z)=0 THEN z:=search
    ELSE
        UpperStr(search)

        MOVE.L offset,A1
        MOVE.L bufsize,D2
        MOVEQ  #0,D0
        MOVEQ  #0,D1
        MOVE.L z,A0
nx2:    MOVE.B (A0)+,D0
        BEQ.S  ex2
        BPL.S  zz2
        ADDQ.L #1,A1
        CMP.L  A1,D2
        BEQ.S  ex2
        BRA.S  nx2
zz2:    CMP.L  A1,D2
        BEQ.S  ex2
        MOVE.B (A1)+,D1
        BCLR   #5,D1
        CMP.B  D0,D1
        BEQ.S  nx2
        MOVE.L search,A0
        BRA.S  nx2
ex2:    MOVE.L A1,offset
        MOVE.L A0,z
        DEC z
        IF (^z)=0 THEN z:=search
        ENDIF
ENDPROC offset-buf
