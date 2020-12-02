CREATE TABLE "queues" (
	"uri"	VARCHAR(64),
	"state"	VARCHAR(16),
	"dequeuer"	VARCHAR(64),
	"max"	INT,
	"length"	INT,
	PRIMARY KEY("uri")
);


