ALIGN_JAR := align.jar

JAVAC := javac
JAR   := jar

SRC   := $(shell ls *.java)
CLASS := $(SRC:.java=.class)
MAN   := MANIFEST.MF

all: $(ALIGN_JAR)

%.class: %.java
	@echo " [JAVAC] $<"
	@$(JAVAC) $<

$(ALIGN_JAR): $(CLASS) $(MAN)
	@echo " [JAR] $@"
	@$(JAR) -cvmf $(MAN) $@ $(CLASS)

clean:
	@echo " [RM] *.class $(ALIGN_JAR)"
	@rm -f *.class $(ALIGN_JAR)

.PHONY: all clean
